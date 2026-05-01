#include "OverlayWindow_win.h"
#include "MessageQueue.h"  // for IPCQueue and IPCCommand

#include <cassert>

// ============================================================================
// Process-wide globals (single-window, single-process application).
// Written before the window is created, never changed after.
// ============================================================================
extern IPCQueue*     g_ipcQueue_win;  // defined in AppController_win.cpp
extern StateManager* g_stateMgr_win; // defined in AppController_win.cpp

// ---------------------------------------------------------------------------
constexpr wchar_t OverlayWindow_win::CLASS_NAME[];

OverlayWindow_win::OverlayWindow_win(StateManager& state, HINSTANCE hInstance)
    : state_(state)
    , hInstance_(hInstance)
{
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        pfnSetWindowDisplayAffinity_ =
            reinterpret_cast<PFN_SetWindowDisplayAffinity>(
                GetProcAddress(hUser32, "SetWindowDisplayAffinity"));
    }
}

OverlayWindow_win::~OverlayWindow_win() {
    Destroy();
}

// ---------------------------------------------------------------------------
// Create
// ---------------------------------------------------------------------------

bool OverlayWindow_win::Create(int x, int y, int width, int height) {
    assert(!hwnd_ && "Create() called twice");

    // --- Register window class ---
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProcStatic;
    wc.hInstance     = hInstance_;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(HOLLOW_BRUSH));
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassExW(&wc)) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            OutputDebugStringW(L"[Overlay-win] RegisterClassExW failed\n");
            return false;
        }
    }
    classRegistered_ = true;

    // --- Extended styles ---
    // WS_EX_LAYERED      — required for opacity (SetLayeredWindowAttributes)
    // WS_EX_TOOLWINDOW   — hidden from taskbar and Alt+Tab
    // WS_EX_TOPMOST      — always on top
    // WS_EX_NOACTIVATE   — do not steal focus
    DWORD exStyle = WS_EX_LAYERED  |
                    WS_EX_TOOLWINDOW |
                    WS_EX_TOPMOST    |
                    WS_EX_NOACTIVATE;

    if (state_.IsClickThrough()) {
        exStyle |= WS_EX_TRANSPARENT;
    }

    DWORD style = WS_POPUP; // borderless

    hwnd_ = CreateWindowExW(
        exStyle,
        CLASS_NAME,
        L"AI Overlay",
        style,
        x, y, width, height,
        nullptr,      // no parent
        nullptr,      // no menu
        hInstance_,
        this          // GWLP_USERDATA set in WM_NCCREATE
    );

    if (!hwnd_) {
        OutputDebugStringW(L"[Overlay-win] CreateWindowExW failed\n");
        return false;
    }

    InitLayeredAttributes();
    ApplyCaptureExclusion();

    return true;
}

// ---------------------------------------------------------------------------
// Destroy
// ---------------------------------------------------------------------------

void OverlayWindow_win::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (classRegistered_ && hInstance_) {
        UnregisterClassW(CLASS_NAME, hInstance_);
        classRegistered_ = false;
    }
}

void OverlayWindow_win::Show() {
    if (hwnd_) {
        ShowWindow(hwnd_, SW_SHOWNA);
        UpdateWindow(hwnd_);
    }
}

void OverlayWindow_win::Hide() {
    if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
}

// ---------------------------------------------------------------------------
// ApplyClickThrough
// ---------------------------------------------------------------------------

void OverlayWindow_win::ApplyClickThrough() {
    if (!hwnd_) return;

    LONG_PTR exStyle = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
    if (state_.IsClickThrough()) {
        exStyle |= WS_EX_TRANSPARENT;
    } else {
        exStyle &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
    }
    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                 SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

// ---------------------------------------------------------------------------
// ApplyOpacity
// ---------------------------------------------------------------------------

void OverlayWindow_win::ApplyOpacity() {
    if (!hwnd_) return;
    BYTE opacity = static_cast<BYTE>(state_.GetOpacity());
    if (!SetLayeredWindowAttributes(hwnd_, 0, opacity, LWA_ALPHA)) {
        OutputDebugStringW(L"[Overlay-win] SetLayeredWindowAttributes failed\n");
    }
}

// ---------------------------------------------------------------------------
// ApplyCaptureExclusion
// ---------------------------------------------------------------------------

bool OverlayWindow_win::ApplyCaptureExclusion() {
    if (!hwnd_) return false;

    if (!pfnSetWindowDisplayAffinity_) {
        OutputDebugStringW(
            L"[Overlay-win] SetWindowDisplayAffinity not available on this OS\n");
        state_.SetCaptureExcluded(false);
        return false;
    }

    // Try WDA_EXCLUDEFROMCAPTURE first (Win10 2004+)
    if (pfnSetWindowDisplayAffinity_(hwnd_, WDA_EXCLUDEFROMCAPTURE_VALUE)) {
        state_.SetCaptureExcluded(true);
        OutputDebugStringW(
            L"[Overlay-win] WDA_EXCLUDEFROMCAPTURE applied\n");
        return true;
    }

    // Fallback: WDA_MONITOR (hides from screen mirroring on older Win10)
    DWORD err = GetLastError();
    wchar_t buf[256];
    swprintf_s(buf,
        L"[Overlay-win] WDA_EXCLUDEFROMCAPTURE failed (%lu), trying WDA_MONITOR\n",
        err);
    OutputDebugStringW(buf);

    if (pfnSetWindowDisplayAffinity_(hwnd_, WDA_MONITOR_VALUE)) {
        state_.SetCaptureExcluded(true);
        OutputDebugStringW(L"[Overlay-win] WDA_MONITOR fallback applied\n");
        return true;
    }

    swprintf_s(buf,
        L"[Overlay-win] WDA_MONITOR also failed (%lu)\n",
        GetLastError());
    OutputDebugStringW(buf);
    state_.SetCaptureExcluded(false);
    return false;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

OsHandle OverlayWindow_win::GetNativeHandle() const noexcept {
    return hwnd_;
}

bool OverlayWindow_win::IsCreated() const noexcept {
    return hwnd_ != nullptr;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void OverlayWindow_win::InitLayeredAttributes() {
    BYTE opacity = static_cast<BYTE>(state_.GetOpacity());
    SetLayeredWindowAttributes(hwnd_, 0, opacity, LWA_ALPHA);
}

// ---------------------------------------------------------------------------
// WndProc (static trampoline → instance)
// ---------------------------------------------------------------------------

LRESULT CALLBACK OverlayWindow_win::WndProcStatic(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    OverlayWindow_win* self = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<OverlayWindow_win*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<OverlayWindow_win*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) return self->WndProc(hwnd, msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT OverlayWindow_win::WndProc(
    HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {

    // -----------------------------------------------------------------------
    // WM_APP_IPC_MESSAGE — WS thread posted this; drain queue on UI thread
    // -----------------------------------------------------------------------
    case WM_APP_IPC_MESSAGE: {
        if (!g_ipcQueue_win) return 0;
        std::vector<IPCCommand> cmds;
        cmds.reserve(8);
        g_ipcQueue_win->TryDrainAll(cmds);
        for (auto& cmd : cmds) {
            std::visit([this](auto&& c) {
                using T = std::decay_t<decltype(c)>;
                if constexpr (std::is_same_v<T, CmdToggleClickthrough>) {
                    state_.ToggleClickThrough();
                    ApplyClickThrough();
                }
                else if constexpr (std::is_same_v<T, CmdSetOpacity>) {
                    state_.SetOpacity(c.value);
                    ApplyOpacity();
                }
            }, cmd);
        }
        return 0;
    }

    // -----------------------------------------------------------------------
    // WM_HOTKEY — dispatched by Windows for our registered hotkeys
    // -----------------------------------------------------------------------
    case WM_HOTKEY: {
        switch (static_cast<int>(wParam)) {
            case HOTKEY_ID_TOGGLE_CLICKTHROUGH:
                state_.ToggleClickThrough();
                ApplyClickThrough();
                break;
            case HOTKEY_ID_TOGGLE_OPACITY:
                state_.ToggleOpacity();
                ApplyOpacity();
                break;
        }
        return 0;
    }

    // -----------------------------------------------------------------------
    // WM_PAINT — transparent fill
    // -----------------------------------------------------------------------
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        FillRect(hdc, &rc,
            static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // prevent default erase / flicker

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}
