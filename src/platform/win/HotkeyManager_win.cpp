#include "HotkeyManager_win.h"

HotkeyManager_win::~HotkeyManager_win() {
    UnregisterAll();
}

bool HotkeyManager_win::RegisterAll(OsHandle hwnd, HotkeyCallback callback) {
    assert(hwnd && "HotkeyManager_win::RegisterAll: null HWND");
    hwnd_     = static_cast<HWND>(hwnd);
    callback_ = std::move(callback);
    bool ok   = true;

    // CTRL + SHIFT + X → toggle click-through
    if (!RegisterHotKey(hwnd_,
                        HOTKEY_ID_TOGGLE_CLICKTHROUGH,
                        MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT,
                        'X'))
    {
        wchar_t buf[128];
        swprintf_s(buf,
            L"[Hotkeys-win] RegisterHotKey CTRL+SHIFT+X failed: %lu\n",
            GetLastError());
        OutputDebugStringW(buf);
        ok = false;
    } else {
        OutputDebugStringW(
            L"[Hotkeys-win] Registered CTRL+SHIFT+X (toggle click-through)\n");
    }

    // CTRL + SHIFT + Z → toggle opacity
    if (!RegisterHotKey(hwnd_,
                        HOTKEY_ID_TOGGLE_OPACITY,
                        MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT,
                        'Z'))
    {
        wchar_t buf[128];
        swprintf_s(buf,
            L"[Hotkeys-win] RegisterHotKey CTRL+SHIFT+Z failed: %lu\n",
            GetLastError());
        OutputDebugStringW(buf);
        ok = false;
    } else {
        OutputDebugStringW(
            L"[Hotkeys-win] Registered CTRL+SHIFT+Z (toggle opacity)\n");
    }

    registered_ = true;
    return ok;
}

void HotkeyManager_win::UnregisterAll() {
    if (!hwnd_ || !registered_) return;
    UnregisterHotKey(hwnd_, HOTKEY_ID_TOGGLE_CLICKTHROUGH);
    UnregisterHotKey(hwnd_, HOTKEY_ID_TOGGLE_OPACITY);
    registered_ = false;
    OutputDebugStringW(L"[Hotkeys-win] All hotkeys unregistered\n");
}

bool HotkeyManager_win::IsRegistered() const noexcept {
    return registered_;
}
