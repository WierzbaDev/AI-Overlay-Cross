#pragma once

#include "Common.h"
#include <functional>

// ============================================================================
// HotkeyCallback
// Called on the UI thread when a hotkey fires.
// ============================================================================
using HotkeyCallback = std::function<void(HotkeyAction)>;

// ============================================================================
// IHotkeyManager
//
// Pure abstract interface implemented separately by:
//   - HotkeyManager_mac.mm  (macOS / CGEventTap)
//   - HotkeyManager_win.cpp (Windows / RegisterHotKey)
//
// RegisterAll() MUST be called from the UI thread.
// ============================================================================
class IHotkeyManager {
public:
    virtual ~IHotkeyManager() = default;

    // Register all hotkeys. Callback is invoked on the UI thread.
    // hwnd is only used on Windows; pass nullptr on macOS.
    virtual bool RegisterAll(OsHandle hwnd, HotkeyCallback callback) = 0;

    // Unregister all hotkeys. Safe to call even if RegisterAll() was never called.
    virtual void UnregisterAll() = 0;

    virtual bool IsRegistered() const noexcept = 0;
};
