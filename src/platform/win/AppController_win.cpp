#include "AppController_win.h"
#include "OverlayWindow_win.h"
#include "HotkeyManager_win.h"
#include "WebSocketServer.h"

#include <memory>

// ============================================================================
// Process-wide globals used by OverlayWindow_win::WndProc
// ============================================================================
IPCQueue*     g_ipcQueue_win = nullptr;
StateManager* g_stateMgr_win = nullptr;

// ============================================================================
// WinImpl — holds concrete Windows subsystems
// ============================================================================
struct AppController_win::WinImpl {
    std::unique_ptr<OverlayWindow_win>  overlay;
    std::unique_ptr<HotkeyManager_win>  hotkeys;
    std::unique_ptr<WebSocketServer>    wsServer;
};

// ============================================================================
// AppController_win
// ============================================================================

AppController_win::AppController_win(HINSTANCE hInstance)
    : hInstance_(hInstance)
    , winImpl_(std::make_unique<WinImpl>())
{
    g_stateMgr_win = &state_;
}

AppController_win::~AppController_win() {
    Shutdown();
    g_ipcQueue_win = nullptr;
    g_stateMgr_win = nullptr;
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------

bool AppController_win::Init() {
    assert(!initialised_ && "Init() called twice");

    // ------------------------------------------------------------------
    // 1. IPC queue
    // ------------------------------------------------------------------
    ipcQueue_ = std::make_unique<IPCQueue>();
    g_ipcQueue_win = ipcQueue_.get();

    // ------------------------------------------------------------------
    // 2. Overlay window
    // ------------------------------------------------------------------
    winImpl_->overlay =
        std::make_unique<OverlayWindow_win>(state_, hInstance_);

    // Use the primary monitor work area
    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    int sw = workArea.right  - workArea.left;
    int sh = workArea.bottom - workArea.top;
    int sx = workArea.left;
    int sy = workArea.top;

    if (!winImpl_->overlay->Create(sx, sy, sw, sh)) {
        OutputDebugStringW(L"[App-win] Failed to create overlay window\n");
        return false;
    }

    HWND hwnd = static_cast<HWND>(winImpl_->overlay->GetNativeHandle());

    // ------------------------------------------------------------------
    // 3. Hotkeys
    // ------------------------------------------------------------------
    winImpl_->hotkeys = std::make_unique<HotkeyManager_win>();

    // Callback is not used (WndProc handles WM_HOTKEY), but we pass a
    // no-op lambda for interface compliance.
    if (!winImpl_->hotkeys->RegisterAll(hwnd, [](HotkeyAction) {})) {
        OutputDebugStringW(
            L"[App-win] One or more hotkeys failed to register (non-fatal)\n");
    }

    // ------------------------------------------------------------------
    // 4. WebSocket server
    //    UINotifier: PostMessage to the overlay HWND
    // ------------------------------------------------------------------
    WebSocketServer::UINotifier notifier = [hwnd]() {
        // Called from WS thread — PostMessage is thread-safe
        PostMessageW(hwnd, WM_APP_IPC_MESSAGE, 0, 0);
    };

    winImpl_->wsServer = std::make_unique<WebSocketServer>(
        *ipcQueue_, std::move(notifier));
    winImpl_->wsServer->port = WS_SERVER_PORT;

    if (!winImpl_->wsServer->Start()) {
        OutputDebugStringW(
            L"[App-win] WebSocket server failed to start (non-fatal)\n");
    }

    // ------------------------------------------------------------------
    // 5. Show
    // ------------------------------------------------------------------
    winImpl_->overlay->Show();

    initialised_ = true;
    OutputDebugStringW(L"[App-win] Initialisation complete\n");
    return true;
}

// ---------------------------------------------------------------------------
// Run — Windows message loop
// ---------------------------------------------------------------------------

int AppController_win::Run() {
    assert(initialised_ && "Run() called before Init()");

    MSG msg{};
    while (true) {
        BOOL ret = GetMessageW(&msg, nullptr, 0, 0);
        if (ret == 0) break;     // WM_QUIT
        if (ret == -1) {
            wchar_t buf[128];
            swprintf_s(buf,
                L"[App-win] GetMessage error: %lu\n",
                GetLastError());
            OutputDebugStringW(buf);
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    OutputDebugStringW(L"[App-win] Message loop exited\n");
    return static_cast<int>(msg.wParam);
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------

void AppController_win::Shutdown() {
    if (shutdown_) return;
    shutdown_ = true;

    if (winImpl_->wsServer) {
        winImpl_->wsServer->Stop();
        winImpl_->wsServer.reset();
    }

    if (ipcQueue_) {
        ipcQueue_->Shutdown();
        ipcQueue_.reset();
    }

    if (winImpl_->hotkeys) {
        winImpl_->hotkeys->UnregisterAll();
        winImpl_->hotkeys.reset();
    }

    if (winImpl_->overlay) {
        winImpl_->overlay->Destroy();
        winImpl_->overlay.reset();
    }

    OutputDebugStringW(L"[App-win] Shutdown complete\n");
}
