// HotkeyManager_mac.mm
// Objective-C++ — compiled only on macOS.

#import <Cocoa/Cocoa.h>
// ApplicationServices provides CGEventTap, CGEventFlags, CGKeyCode etc.
// Do NOT import Carbon/Carbon.h — it is deprecated on macOS 12+ and its
// HIToolbox headers conflict with some SDK versions.
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>

#include <cstdio>
#include <memory>

#include "HotkeyManager_mac.h"

// ============================================================================
// Hotkey definitions
//
// CTRL + SHIFT + X → toggle click-through
// CTRL + SHIFT + Z → toggle opacity
//
// CGEventTap receives raw keyDown events. We check the modifier flags and
// the virtual key code.
//
// Virtual key codes (Carbon HIToolbox/Events.h):
//   kVK_ANSI_X = 0x07
//   kVK_ANSI_Z = 0x06
// ============================================================================
static constexpr CGKeyCode kVK_X = 0x07;
static constexpr CGKeyCode kVK_Z = 0x06;

static constexpr CGEventFlags kCtrlShift =
    kCGEventFlagMaskControl | kCGEventFlagMaskShift;

// Mask of all modifier bits we care about (ignore Caps Lock, Fn, Num Lock)
static constexpr CGEventFlags kModifierMask =
    kCGEventFlagMaskControl  |
    kCGEventFlagMaskShift    |
    kCGEventFlagMaskAlternate |
    kCGEventFlagMaskCommand;

// ============================================================================
// Pimpl
// ============================================================================
struct HotkeyManager_mac::Impl {
    CFMachPortRef  eventTap  = nullptr;
    CFRunLoopSourceRef runLoopSource = nullptr;
    HotkeyCallback callback;
    bool           registered = false;

    // Static trampoline for CGEventTapCallback
    static CGEventRef EventTapCallback(
        CGEventTapProxy /*proxy*/,
        CGEventType      type,
        CGEventRef       event,
        void*            userInfo)
    {
        auto* self = static_cast<Impl*>(userInfo);
        return self->HandleEvent(type, event);
    }

    CGEventRef HandleEvent(CGEventType type, CGEventRef event) {
        // If the tap was disabled by the system (e.g. accessibility revoked),
        // re-enable it.
        if (type == kCGEventTapDisabledByTimeout ||
            type == kCGEventTapDisabledByUserInput)
        {
            if (eventTap) {
                CGEventTapEnable(eventTap, true);
            }
            return event;
        }

        if (type != kCGEventKeyDown) {
            return event;
        }

        CGEventFlags flags    = CGEventGetFlags(event);
        CGKeyCode    keyCode  = static_cast<CGKeyCode>(
            CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));

        // Mask out non-modifier bits
        CGEventFlags maskedFlags = flags & kModifierMask;

        if (maskedFlags == kCtrlShift) {
            if (keyCode == kVK_X) {
                // Fire on main thread to stay thread-safe with UI
                if (callback) {
                    HotkeyCallback cb = callback; // capture by value
                    dispatch_async(dispatch_get_main_queue(), ^{
                        cb(HotkeyAction::ToggleClickThrough);
                    });
                }
                // Swallow the event so it does not reach the focused app
                return nullptr;
            }
            if (keyCode == kVK_Z) {
                if (callback) {
                    HotkeyCallback cb = callback;
                    dispatch_async(dispatch_get_main_queue(), ^{
                        cb(HotkeyAction::ToggleOpacity);
                    });
                }
                return nullptr;
            }
        }

        return event; // pass all other events through
    }
};

// ============================================================================
// HotkeyManager_mac
// ============================================================================

HotkeyManager_mac::HotkeyManager_mac()
    : impl_(std::make_unique<Impl>())
{}

HotkeyManager_mac::~HotkeyManager_mac() {
    UnregisterAll();
}

bool HotkeyManager_mac::RegisterAll(OsHandle /*hwnd*/, HotkeyCallback callback) {
    if (impl_->registered) return true;

    // ------------------------------------------------------------------
    // Check / request Accessibility access (required for CGEventTap)
    // ------------------------------------------------------------------
    {
        NSDictionary* opts = @{(__bridge id)kAXTrustedCheckOptionPrompt: @YES};
        bool trusted = AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)opts);
        if (!trusted) {
            fprintf(stderr,
                "[Hotkeys-mac] Accessibility permission not granted. "
                "Grant access in System Preferences → Privacy → Accessibility "
                "and restart the application.\n");
            // We still try to install the tap; on some OS versions it works
            // if the app is in the trusted list but the flag hasn't propagated yet.
        }
    }

    impl_->callback = std::move(callback);

    // ------------------------------------------------------------------
    // Create the event tap
    // Listen for keyDown events at the session level (system-wide)
    // ------------------------------------------------------------------
    CGEventMask eventMask = CGEventMaskBit(kCGEventKeyDown);

    impl_->eventTap = CGEventTapCreate(
        kCGSessionEventTap,          // intercept at session level
        kCGHeadInsertEventTap,       // insert at head of tap list
        kCGEventTapOptionDefault,    // active tap (can swallow events)
        eventMask,
        Impl::EventTapCallback,
        impl_.get()
    );

    if (!impl_->eventTap) {
        fprintf(stderr,
            "[Hotkeys-mac] CGEventTapCreate failed. "
            "Ensure Accessibility permission is granted.\n");
        return false;
    }

    // ------------------------------------------------------------------
    // Add to the main run loop so events are processed on the UI thread
    // (the callback itself dispatches back to main queue, but the tap
    // must be scheduled on a run loop)
    // ------------------------------------------------------------------
    impl_->runLoopSource = CFMachPortCreateRunLoopSource(
        kCFAllocatorDefault,
        impl_->eventTap,
        0);

    if (!impl_->runLoopSource) {
        fprintf(stderr, "[Hotkeys-mac] CFMachPortCreateRunLoopSource failed\n");
        CFRelease(impl_->eventTap);
        impl_->eventTap = nullptr;
        return false;
    }

    CFRunLoopAddSource(
        CFRunLoopGetMain(),
        impl_->runLoopSource,
        kCFRunLoopCommonModes);

    CGEventTapEnable(impl_->eventTap, true);

    impl_->registered = true;
    fprintf(stderr,
        "[Hotkeys-mac] CGEventTap registered: "
        "CTRL+SHIFT+X (click-through), CTRL+SHIFT+Z (opacity)\n");
    return true;
}

void HotkeyManager_mac::UnregisterAll() {
    if (!impl_->registered) return;

    if (impl_->eventTap) {
        CGEventTapEnable(impl_->eventTap, false);
    }

    if (impl_->runLoopSource) {
        CFRunLoopRemoveSource(
            CFRunLoopGetMain(),
            impl_->runLoopSource,
            kCFRunLoopCommonModes);
        CFRelease(impl_->runLoopSource);
        impl_->runLoopSource = nullptr;
    }

    if (impl_->eventTap) {
        CFRelease(impl_->eventTap);
        impl_->eventTap = nullptr;
    }

    impl_->registered = false;
    fprintf(stderr, "[Hotkeys-mac] CGEventTap unregistered\n");
}

bool HotkeyManager_mac::IsRegistered() const noexcept {
    return impl_->registered;
}
