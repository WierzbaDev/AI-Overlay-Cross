#pragma once

#include "Common.h"
#include "StateManager.h"
#include "MessageQueue.h"

// ============================================================================
// AppController_mac
//
// Orchestrates all subsystems on macOS.
// The Objective-C++ implementation is in AppController_mac.mm.
// ============================================================================
class AppController_mac {
public:
    struct Impl;

    AppController_mac();
    ~AppController_mac();

    AppController_mac(const AppController_mac&) = delete;
    AppController_mac& operator=(const AppController_mac&) = delete;

    // Initialise all subsystems. Must be called from the main thread.
    // Returns false on fatal error.
    bool Init();

    // Enter NSApplication run loop. Returns when the app terminates.
    int Run();

    // Clean shutdown. Called automatically by destructor.
    void Shutdown();

private:
    std::unique_ptr<Impl> impl_;
};
