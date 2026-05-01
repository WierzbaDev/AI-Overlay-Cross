// main_win.cpp — Windows entry point

#include "Common.h"
#include "AppController_win.h"

// ============================================================================
// Single-instance mutex
// ============================================================================
static constexpr wchar_t MUTEX_NAME[] =
    L"Global\\AIOverlayCross_SingleInstance_Mutex";

// ============================================================================
// WinMain
// ============================================================================
int WINAPI wWinMain(
    _In_     HINSTANCE hInstance,
    _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_     LPWSTR    /*lpCmdLine*/,
    _In_     int       /*nCmdShow*/)
{
    // ------------------------------------------------------------------
    // 1. Single-instance guard
    // ------------------------------------------------------------------
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, MUTEX_NAME);
    if (!hMutex) {
        MessageBoxW(nullptr,
            L"Failed to create instance mutex.",
            L"AI Overlay — Fatal Error",
            MB_ICONERROR | MB_OK);
        return 1;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMutex);
        MessageBoxW(nullptr,
            L"AI Overlay is already running.",
            L"AI Overlay",
            MB_ICONINFORMATION | MB_OK);
        return 0;
    }

    // ------------------------------------------------------------------
    // 2. Per-monitor DPI awareness (Win10+, loaded dynamically)
    // ------------------------------------------------------------------
    {
        using PFN = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        HMODULE hU32 = GetModuleHandleW(L"user32.dll");
        if (hU32) {
            auto pfn = reinterpret_cast<PFN>(
                GetProcAddress(hU32, "SetProcessDpiAwarenessContext"));
            if (pfn) {
                // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 = -4
                pfn(reinterpret_cast<DPI_AWARENESS_CONTEXT>(-4));
            }
        }
    }

    // ------------------------------------------------------------------
    // 3. Run the application
    // ------------------------------------------------------------------
    int exitCode = 0;
    {
        AppController_win app(hInstance);
        if (!app.Init()) {
            MessageBoxW(nullptr,
                L"Failed to initialise AI Overlay.\n"
                L"Check debug output for details.",
                L"AI Overlay — Fatal Error",
                MB_ICONERROR | MB_OK);
            ReleaseMutex(hMutex);
            CloseHandle(hMutex);
            return 1;
        }
        exitCode = app.Run();
        // destructor calls Shutdown()
    }

    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    return exitCode;
}
