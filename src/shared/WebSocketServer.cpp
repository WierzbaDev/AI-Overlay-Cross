#include "WebSocketServer.h"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <string>
#include <vector>

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace websocket = beast::websocket;
using tcp = asio::ip::tcp;

static void WSLog(const char* msg) {
#if PLATFORM_WINDOWS
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");
#else
    fprintf(stderr, "%s\n", msg);
#endif
}

static void WSLogF(const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    WSLog(buf);
}

class WebSocketSession;

struct WebSocketServer::Impl {
    asio::io_context io;
    tcp::acceptor acceptor{io};
    std::mutex sessionsMutex;
    std::vector<std::shared_ptr<WebSocketSession>> sessions;
};

class WebSocketSession : public std::enable_shared_from_this<WebSocketSession> {
public:
    WebSocketSession(tcp::socket socket, WebSocketServer& owner)
        : ws_(std::move(socket))
        , owner_(owner)
    {}

    void Run() {
        beast::error_code ec;
        ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::server));
        ws_.accept(ec);
        if (ec) {
            WSLogF("[WS] handshake failed: %s", ec.message().c_str());
            owner_.UnregisterSession(this);
            return;
        }

        WSLog("[WS] Client connected");

        for (;;) {
            beast::flat_buffer buffer;
            ws_.read(buffer, ec);
            if (ec == websocket::error::closed) {
                break;
            }
            if (ec) {
                WSLogF("[WS] read failed: %s", ec.message().c_str());
                break;
            }

            const std::string payload = beast::buffers_to_string(buffer.data());
            const std::string response = owner_.HandlePayload(payload);

            ws_.text(true);
            ws_.write(asio::buffer(response), ec);
            if (ec) {
                WSLogF("[WS] write failed: %s", ec.message().c_str());
                break;
            }
        }

        Close();
        owner_.UnregisterSession(this);
        WSLog("[WS] Client disconnected");
    }

    void Close() {
        std::lock_guard<std::mutex> lock(closeMutex_);
        beast::error_code ec;
        ws_.close(websocket::close_code::going_away, ec);
        beast::get_lowest_layer(ws_).close(ec);
    }

private:
    websocket::stream<tcp::socket> ws_;
    WebSocketServer& owner_;
    std::mutex closeMutex_;
};

WebSocketServer::WebSocketServer(IPCQueue& queue, UINotifier notifier)
    : queue_(queue)
    , uiNotifier_(std::move(notifier))
    , impl_(std::make_unique<Impl>())
{}

WebSocketServer::~WebSocketServer() {
    Stop();
}

bool WebSocketServer::Start() {
    if (running_.load(std::memory_order_acquire)) return true;

    beast::error_code ec;
    const auto address = asio::ip::make_address(WS_SERVER_HOST, ec);
    if (ec) {
        WSLogF("[WS] invalid bind address %s: %s", WS_SERVER_HOST, ec.message().c_str());
        return false;
    }

    tcp::endpoint endpoint(address, port);
    impl_->acceptor.open(endpoint.protocol(), ec);
    if (ec) {
        WSLogF("[WS] open failed: %s", ec.message().c_str());
        return false;
    }

    impl_->acceptor.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec) {
        WSLogF("[WS] set reuse_addr failed: %s", ec.message().c_str());
        return false;
    }

    impl_->acceptor.bind(endpoint, ec);
    if (ec) {
        WSLogF("[WS] listen bind %s:%u failed: %s",
               WS_SERVER_HOST,
               static_cast<unsigned>(port),
               ec.message().c_str());
        return false;
    }

    impl_->acceptor.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) {
        WSLogF("[WS] listen failed: %s", ec.message().c_str());
        return false;
    }

    stopping_.store(false, std::memory_order_release);
    running_.store(true, std::memory_order_release);
    thread_ = std::thread([this]() { ServerThread(); });

    WSLogF("[WS] Server started on %s:%u", WS_SERVER_HOST, static_cast<unsigned>(port));
    return true;
}

void WebSocketServer::Stop() {
    if (!running_.load(std::memory_order_acquire)) return;

    stopping_.store(true, std::memory_order_release);

    beast::error_code ec;
    impl_->acceptor.close(ec);

    std::vector<std::shared_ptr<WebSocketSession>> sessions;
    {
        std::lock_guard<std::mutex> lock(impl_->sessionsMutex);
        sessions = impl_->sessions;
    }

    for (const auto& session : sessions) {
        if (session) session->Close();
    }

    if (thread_.joinable()) thread_.join();

    {
        std::lock_guard<std::mutex> lock(impl_->sessionsMutex);
        impl_->sessions.clear();
    }

    running_.store(false, std::memory_order_release);
    WSLog("[WS] Server stopped");
}

void WebSocketServer::ServerThread() {
    while (!stopping_.load(std::memory_order_acquire)) {
        beast::error_code ec;
        tcp::socket socket(impl_->io);
        impl_->acceptor.accept(socket, ec);

        if (ec) {
            if (!stopping_.load(std::memory_order_acquire)) {
                WSLogF("[WS] accept failed: %s", ec.message().c_str());
            }
            break;
        }

        auto session = std::make_shared<WebSocketSession>(std::move(socket), *this);
        RegisterSession(session);
        std::thread([session]() { session->Run(); }).detach();
    }

    running_.store(false, std::memory_order_release);
}

void WebSocketServer::RegisterSession(const std::shared_ptr<WebSocketSession>& session) {
    std::lock_guard<std::mutex> lock(impl_->sessionsMutex);
    impl_->sessions.push_back(session);
}

void WebSocketServer::UnregisterSession(const WebSocketSession* session) {
    std::lock_guard<std::mutex> lock(impl_->sessionsMutex);
    impl_->sessions.erase(
        std::remove_if(
            impl_->sessions.begin(),
            impl_->sessions.end(),
            [session](const std::shared_ptr<WebSocketSession>& candidate) {
                return candidate.get() == session;
            }),
        impl_->sessions.end());
}

std::string WebSocketServer::HandlePayload(const std::string& payload) {
    if (stopping_.load(std::memory_order_acquire)) {
        return nlohmann::json({{"status", "error"}, {"message", "server stopping"}}).dump();
    }

    if (payload.empty()) {
        WSLog("[WS] Empty message ignored");
        return nlohmann::json({{"status", "error"}, {"message", "empty message"}}).dump();
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(payload);
    } catch (const nlohmann::json::parse_error& e) {
        WSLogF("[WS] JSON parse error: %s", e.what());
        return nlohmann::json({
            {"status", "error"},
            {"message", "invalid JSON"},
            {"detail", e.what()}
        }).dump();
    }

    if (!j.is_object()) {
        WSLog("[WS] JSON root is not an object");
        return nlohmann::json({{"status", "error"}, {"message", "root must be a JSON object"}}).dump();
    }

    if (!j.contains("type") || !j["type"].is_string()) {
        WSLog("[WS] Missing or non-string 'type' field");
        return nlohmann::json({{"status", "error"}, {"message", "missing 'type' field"}}).dump();
    }

    DispatchJson(j);
    return nlohmann::json({{"status", "ok"}}).dump();
}

void WebSocketServer::DispatchJson(const nlohmann::json& j) {
    const std::string type = j["type"].get<std::string>();

    if (type == "toggle_clickthrough") {
        queue_.Push(CmdToggleClickthrough{});
        uiNotifier_();
        WSLog("[WS] Command: toggle_clickthrough");
    }
    else if (type == "set_opacity") {
        if (!j.contains("value")) {
            WSLog("[WS] set_opacity: missing 'value' field");
            return;
        }
        const auto& valField = j["value"];
        if (!valField.is_number_integer() && !valField.is_number_unsigned()) {
            WSLog("[WS] set_opacity: 'value' is not an integer");
            return;
        }
        int raw = valField.get<int>();
        uint8_t clamped = static_cast<uint8_t>(std::clamp(raw, 0, 255));
        queue_.Push(CmdSetOpacity{clamped});
        uiNotifier_();
        WSLogF("[WS] Command: set_opacity(%d)", static_cast<int>(clamped));
    }
    else {
        WSLogF("[WS] Unknown message type: %s", type.c_str());
    }
}
