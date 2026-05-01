#pragma once

#include "Common.h"
#include "MessageQueue.h"

#if PLATFORM_WINDOWS
  #ifndef _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
    #define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
  #endif
#endif

#include <nlohmann/json.hpp>

#include <atomic>
#include <thread>
#include <functional>
#include <memory>

// ============================================================================
// WebSocketServer
//
// Runs on its own background thread.
// Parses JSON messages and pushes IPCCommand objects into an IPCQueue.
// After each Push(), calls the platform notifier (set via SetUINotifier) so
// the UI thread wakes up and drains the queue.
//
// Platform-specific notifier:
//   Windows → posts WM_APP_IPC_MESSAGE via PostMessage
//   macOS   → dispatch_async(dispatch_get_main_queue(), ...)
//
// All of this is injected so WebSocketServer itself stays platform-agnostic.
// ============================================================================
class WebSocketServer {
public:
    // uiNotifier is called (from the WS thread) after each command is queued.
    // It must be thread-safe and non-blocking.
    using UINotifier = std::function<void()>;

    WebSocketServer(IPCQueue& queue, UINotifier notifier);
    ~WebSocketServer();

    WebSocketServer(const WebSocketServer&) = delete;
    WebSocketServer& operator=(const WebSocketServer&) = delete;

    // Start listening. Spawns background thread.
    bool Start();

    // Stop and join the background thread.
    void Stop();

    bool IsRunning() const noexcept { return running_.load(std::memory_order_acquire); }

    uint16_t port = WS_SERVER_PORT;

private:
    friend class WebSocketSession;
    struct Impl;
    void ServerThread();
    std::string HandlePayload(const std::string& payload);
    void RegisterSession(const std::shared_ptr<class WebSocketSession>& session);
    void UnregisterSession(const class WebSocketSession* session);
    void DispatchJson(const nlohmann::json& j);

    IPCQueue&         queue_;
    UINotifier        uiNotifier_;
    std::unique_ptr<Impl> impl_;
    std::thread       thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopping_{false};
};
