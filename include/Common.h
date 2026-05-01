#pragma once

// ============================================================================
// Platform detection
// ============================================================================
#if defined(_WIN32) || defined(_WIN64)
  #define PLATFORM_WINDOWS 1
  #define PLATFORM_MACOS   0
#elif defined(__APPLE__) && defined(__MACH__)
  #define PLATFORM_WINDOWS 0
  #define PLATFORM_MACOS   1
#else
  #error "Unsupported platform. Only Windows and macOS are supported."
#endif

// ============================================================================
// Windows headers (included only when compiling for Windows)
// ============================================================================
#if PLATFORM_WINDOWS
  #ifndef WINVER
    #define WINVER 0x0A00
  #endif
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0A00
  #endif
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <windows.h>
  using OsHandle = HWND;
#endif

// ============================================================================
// macOS: only forward-declare what we need in C++ headers.
// Objective-C types are used only inside .mm files.
// ============================================================================
#if PLATFORM_MACOS
  // OsHandle is a void* on macOS (will hold NSWindow* cast)
  using OsHandle = void*;
#endif

// ============================================================================
// Standard library
// ============================================================================
#include <cstdint>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <thread>
#include <memory>
#include <stdexcept>
#include <cassert>
#include <variant>
#include <algorithm>

// ============================================================================
// Opacity presets (0-255 scale; used on both platforms)
// ============================================================================
constexpr uint8_t OPACITY_LOW  = 80;   // ~31%
constexpr uint8_t OPACITY_HIGH = 220;  // ~86%

// ============================================================================
// IPC
// ============================================================================
constexpr uint16_t    WS_SERVER_PORT = 8765;
constexpr const char* WS_SERVER_HOST = "127.0.0.1";

// ============================================================================
// Hotkey action identifiers (platform-agnostic)
// ============================================================================
enum class HotkeyAction : int {
    ToggleClickThrough = 1,
    ToggleOpacity      = 2,
};

// ============================================================================
// Windows-specific message IDs (WM_APP range)
// ============================================================================
#if PLATFORM_WINDOWS
  constexpr UINT WM_APP_IPC_MESSAGE  = WM_APP + 1;
  constexpr UINT WM_APP_TOGGLE_CLICK = WM_APP + 2;
  constexpr UINT WM_APP_SET_OPACITY  = WM_APP + 3;
  constexpr int  HOTKEY_ID_TOGGLE_CLICKTHROUGH = 1;
  constexpr int  HOTKEY_ID_TOGGLE_OPACITY      = 2;
#endif
