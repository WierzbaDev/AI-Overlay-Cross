#pragma once

#include "IOverlayWindow.h"
#include "StateManager.h"

// ============================================================================
// OverlayWindow_mac
//
// macOS implementation of IOverlayWindow using AppKit (NSWindow).
// The Objective-C++ implementation lives in OverlayWindow_mac.mm.
//
// This header is pure C++ so it can be included from C++ translation units.
// The NSWindow* is held inside an opaque Pimpl to avoid leaking ObjC types.
// ============================================================================
class OverlayWindow_mac : public IOverlayWindow {
public:
    explicit OverlayWindow_mac(StateManager& state);
    ~OverlayWindow_mac() override;

    OverlayWindow_mac(const OverlayWindow_mac&) = delete;
    OverlayWindow_mac& operator=(const OverlayWindow_mac&) = delete;

    bool    Create(int x, int y, int width, int height) override;
    void    Destroy()                                   override;
    void    Show()                                      override;
    void    Hide()                                      override;
    void    ApplyClickThrough()                         override;
    void    ApplyOpacity()                              override;
    bool    ApplyCaptureExclusion()                     override;

    OsHandle GetNativeHandle() const noexcept           override;
    bool     IsCreated()       const noexcept           override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    StateManager&         state_;
};
