#pragma once

#include "IOverlayWindow.h"
#include "StateManager.h"

// ============================================================================
// OverlayWindow_win
//
// Windows implementation of IOverlayWindow using Win32 / WinAPI.
// ============================================================================
class OverlayWindow_win : public IOverlayWindow {
public:
    explicit OverlayWindow_win(StateManager& state, HINSTANCE hInstance);
    ~OverlayWindow_win() override;

    OverlayWindow_win(const OverlayWindow_win&) = delete;
    OverlayWindow_win& operator=(const OverlayWindow_win&) = delete;

    bool    Create(int x, int y, int width, int height)  override;
    void    Destroy()                                     override;
    void    Show()                                        override;
    void    Hide()                                        override;
    void    ApplyClickThrough()                           override;
    void    ApplyOpacity()                                override;
    bool    ApplyCaptureExclusion()                       override;

    OsHandle GetNativeHandle() const noexcept             override;
    bool     IsCreated()       const noexcept             override;

    static constexpr wchar_t CLASS_NAME[] = L"AIOverlayWindowClass";

private:
    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg,
                                          WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void InitLayeredAttributes();

    StateManager& state_;
    HINSTANCE     hInstance_     = nullptr;
    HWND          hwnd_          = nullptr;
    bool          classRegistered_ = false;

    // Dynamically loaded — WDA_EXCLUDEFROMCAPTURE requires Win10 2004+
    using PFN_SetWindowDisplayAffinity = BOOL(WINAPI*)(HWND, DWORD);
    PFN_SetWindowDisplayAffinity pfnSetWindowDisplayAffinity_ = nullptr;

    static constexpr DWORD WDA_EXCLUDEFROMCAPTURE_VALUE = 0x00000011;
    static constexpr DWORD WDA_MONITOR_VALUE            = 0x00000001;
};
