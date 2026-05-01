#pragma once

#include "Common.h"

// ============================================================================
// IOverlayWindow
//
// Pure abstract interface implemented separately by:
//   - OverlayWindow_mac.mm  (macOS / AppKit)
//   - OverlayWindow_win.cpp (Windows / Win32)
//
// All methods must be called from the UI thread unless stated otherwise.
// ============================================================================
class IOverlayWindow {
public:
    virtual ~IOverlayWindow() = default;

    // Create the native window. Returns false on failure.
    virtual bool Create(int x, int y, int width, int height) = 0;

    // Destroy the native window.
    virtual void Destroy() = 0;

    // Show / hide without stealing focus.
    virtual void Show() = 0;
    virtual void Hide() = 0;

    // Sync click-through from StateManager to the native window.
    virtual void ApplyClickThrough() = 0;

    // Sync opacity from StateManager to the native window.
    virtual void ApplyOpacity() = 0;

    // Apply best-effort screen-capture exclusion.
    // Returns true if fully excluded.
    virtual bool ApplyCaptureExclusion() = 0;

    // Returns the native window handle (HWND on Windows, NSWindow* on macOS).
    virtual OsHandle GetNativeHandle() const noexcept = 0;

    virtual bool IsCreated() const noexcept = 0;
};
