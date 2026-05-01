#pragma once

#include "Common.h"
#include "StateManager.h"
#include "MessageQueue.h"

// ============================================================================
// AppController_win
//
// Orchestrates all Windows subsystems:
//   StateManager, OverlayWindow_win, HotkeyManager_win, WebSocketServer
// ============================================================================
class AppController_win {
public:
    explicit AppController_win(HINSTANCE hInstance);
    ~AppController_win();

    AppController_win(const AppController_win&) = delete;
    AppController_win& operator=(const AppController_win&) = delete;

    bool Init();
    int  Run();
    void Shutdown();

private:
    HINSTANCE hInstance_;

    StateManager                         state_;
    std::unique_ptr<IPCQueue>            ipcQueue_;

    // Concrete types (forward-declared via unique_ptr)
    struct WinImpl;
    std::unique_ptr<WinImpl>             winImpl_;

    bool initialised_ = false;
    bool shutdown_    = false;
};

// Globals used by OverlayWindow_win.cpp
extern IPCQueue*     g_ipcQueue_win;
extern StateManager* g_stateMgr_win;
