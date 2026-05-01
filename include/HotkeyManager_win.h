#pragma once

#include "IHotkeyManager.h"

// ============================================================================
// HotkeyManager_win
//
// Windows implementation of IHotkeyManager using RegisterHotKey.
// WM_HOTKEY is handled in OverlayWindow_win::WndProc — the callback
// here is provided for consistency with the interface but WM_HOTKEY
// processing happens directly in the window procedure for efficiency.
// ============================================================================
class HotkeyManager_win : public IHotkeyManager {
public:
    HotkeyManager_win() = default;
    ~HotkeyManager_win() override;

    HotkeyManager_win(const HotkeyManager_win&) = delete;
    HotkeyManager_win& operator=(const HotkeyManager_win&) = delete;

    // hwnd: the overlay window that will receive WM_HOTKEY messages.
    // callback: stored but not used (WndProc handles WM_HOTKEY directly).
    bool RegisterAll(OsHandle hwnd, HotkeyCallback callback) override;
    void UnregisterAll()                                      override;
    bool IsRegistered() const noexcept                        override;

private:
    HWND          hwnd_      = nullptr;
    bool          registered_ = false;
    HotkeyCallback callback_; // stored for completeness
};
