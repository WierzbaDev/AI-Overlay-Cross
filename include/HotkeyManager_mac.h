#pragma once

#include "IHotkeyManager.h"

// ============================================================================
// HotkeyManager_mac
//
// Global hotkey implementation using CGEventTap.
// CGEventTap intercepts keyboard events system-wide, even when the
// application is not focused.
//
// Requires Accessibility permission (System Preferences → Privacy →
// Accessibility). The implementation requests permission and logs a clear
// error if it is denied.
//
// Implementation is in HotkeyManager_mac.mm (Objective-C++).
// ============================================================================
class HotkeyManager_mac : public IHotkeyManager {
public:
    HotkeyManager_mac();
    ~HotkeyManager_mac() override;

    HotkeyManager_mac(const HotkeyManager_mac&) = delete;
    HotkeyManager_mac& operator=(const HotkeyManager_mac&) = delete;

    bool RegisterAll(OsHandle hwnd, HotkeyCallback callback) override;
    void UnregisterAll()                                      override;
    bool IsRegistered() const noexcept                        override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
