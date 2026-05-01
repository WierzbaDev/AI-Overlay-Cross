// OverlayWindow_mac.mm
// Objective-C++ — compiled only on macOS.

#import <Cocoa/Cocoa.h>
#import <AppKit/AppKit.h>
#include <cstdio>

#include "OverlayWindow_mac.h"

// ============================================================================
// AIOverlayWindowDelegate — receives NSWindow delegate callbacks
// ============================================================================
@interface AIOverlayWindowDelegate : NSObject <NSWindowDelegate>
@end

@implementation AIOverlayWindowDelegate

- (BOOL)windowShouldClose:(NSWindow*)sender {
    // Do not allow closing via keyboard shortcut or programmatic close request.
    // The window lifetime is managed by OverlayWindow_mac.
    (void)sender;
    return NO;
}

- (void)windowDidChangeScreen:(NSNotification*)notification {
    // Re-apply the window level when the screen changes (e.g. moving to
    // a different monitor) to ensure always-on-top over fullscreen is maintained.
    // Must match the level set in Create() — NSStatusWindowLevel.
    NSWindow* win = (NSWindow*)notification.object;
    [win setLevel:NSStatusWindowLevel];
}

@end

// ============================================================================
// AIOverlayView — custom NSView for the overlay content area.
// Draws a fully transparent background; real content will be the Flutter UI.
// ============================================================================
@interface AIOverlayView : NSView
@end

@implementation AIOverlayView

- (BOOL)isOpaque {
    return NO;
}

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor clearColor] set];
    NSRectFill(dirtyRect);
}

// Do not accept first responder so the overlay does not steal keyboard focus.
- (BOOL)acceptsFirstResponder {
    return NO;
}

@end

// ============================================================================
// Pimpl struct — holds all Objective-C objects
// ============================================================================
struct OverlayWindow_mac::Impl {
    NSWindow*                  window   = nil;
    AIOverlayView*             view     = nil;
    AIOverlayWindowDelegate*   delegate = nil;
};

// ============================================================================
// OverlayWindow_mac implementation
// ============================================================================

OverlayWindow_mac::OverlayWindow_mac(StateManager& state)
    : impl_(std::make_unique<Impl>())
    , state_(state)
{}

OverlayWindow_mac::~OverlayWindow_mac() {
    Destroy();
}

bool OverlayWindow_mac::Create(int x, int y, int width, int height) {
    @autoreleasepool {
        if (impl_->window) {
            fprintf(stderr, "[Overlay-mac] Create() called on already-created window\n");
            return false;
        }

        // ------------------------------------------------------------------
        // 1. Window style mask: borderless, non-opaque
        // ------------------------------------------------------------------
        NSWindowStyleMask styleMask = NSWindowStyleMaskBorderless;

        NSRect contentRect = NSMakeRect(
            static_cast<CGFloat>(x),
            static_cast<CGFloat>(y),
            static_cast<CGFloat>(width),
            static_cast<CGFloat>(height));

        impl_->window = [[NSWindow alloc]
            initWithContentRect : contentRect
            styleMask           : styleMask
            backing             : NSBackingStoreBuffered
            defer               : NO];

        if (!impl_->window) {
            fprintf(stderr, "[Overlay-mac] NSWindow alloc/init failed\n");
            return false;
        }

        // ------------------------------------------------------------------
        // 2. Transparent background
        // ------------------------------------------------------------------
        [impl_->window setBackgroundColor:[NSColor clearColor]];
        [impl_->window setOpaque:NO];

        // ------------------------------------------------------------------
        // 3. Always-on-top: NSFloatingWindowLevel keeps the overlay above
        //    normal windows. NSStatusWindowLevel (25) keeps it above
        //    fullscreen apps.  We use NSStatusWindowLevel to match the
        //    requirement of being visible over fullscreen apps.
        // ------------------------------------------------------------------
        [impl_->window setLevel:NSStatusWindowLevel];

        // ------------------------------------------------------------------
        // 4. Visible across all Spaces and fullscreen apps
        // NSWindowCollectionBehaviorCanJoinAllSpaces — present on all desktops
        // NSWindowCollectionBehaviorFullScreenAuxiliary — visible over fullscreen
        // NSWindowCollectionBehaviorStationary — does not move with Spaces swipes
        // ------------------------------------------------------------------
        NSWindowCollectionBehavior behavior =
            NSWindowCollectionBehaviorCanJoinAllSpaces      |
            NSWindowCollectionBehaviorFullScreenAuxiliary   |
            NSWindowCollectionBehaviorStationary            |
            NSWindowCollectionBehaviorIgnoresCycle;          // hide from Cmd+Tab

        [impl_->window setCollectionBehavior:behavior];

        // ------------------------------------------------------------------
        // 5. Excluded from screen capture
        //    NSWindowSharingNone prevents the window content from appearing
        //    in screen recordings and screenshots taken by other apps.
        // ------------------------------------------------------------------
        [impl_->window setSharingType:NSWindowSharingNone];

        // ------------------------------------------------------------------
        // 6. Do not appear in the Dock or Cmd+Tab switcher.
        //    NSApplicationActivationPolicyAccessory is set on the application
        //    in AppController_mac.mm; the window-level exclusion is handled
        //    by NSWindowCollectionBehaviorIgnoresCycle above.
        // ------------------------------------------------------------------

        // ------------------------------------------------------------------
        // 7. Content view
        // ------------------------------------------------------------------
        impl_->view = [[AIOverlayView alloc] initWithFrame:contentRect];
        [impl_->window setContentView:impl_->view];

        // ------------------------------------------------------------------
        // 8. Delegate
        // ------------------------------------------------------------------
        impl_->delegate = [[AIOverlayWindowDelegate alloc] init];
        [impl_->window setDelegate:impl_->delegate];

        // ------------------------------------------------------------------
        // 9. Additional window properties
        // ------------------------------------------------------------------
        [impl_->window setIgnoresMouseEvents:state_.IsClickThrough()];
        [impl_->window setHasShadow:NO];
        [impl_->window setMovableByWindowBackground:NO];

        // Apply initial opacity
        double alpha = static_cast<double>(state_.GetOpacity()) / 255.0;
        [impl_->window setAlphaValue:alpha];

        fprintf(stderr, "[Overlay-mac] Window created (%d,%d) %dx%d\n",
                x, y, width, height);
        return true;
    }
}

void OverlayWindow_mac::Destroy() {
    @autoreleasepool {
        if (impl_->window) {
            [impl_->window setDelegate:nil];
            [impl_->window close];
            impl_->window   = nil;
            impl_->view     = nil;
            impl_->delegate = nil;
            fprintf(stderr, "[Overlay-mac] Window destroyed\n");
        }
    }
}

void OverlayWindow_mac::Show() {
    @autoreleasepool {
        if (impl_->window) {
            // orderFrontRegardless shows the window without activating the app.
            [impl_->window orderFrontRegardless];
        }
    }
}

void OverlayWindow_mac::Hide() {
    @autoreleasepool {
        if (impl_->window) {
            [impl_->window orderOut:nil];
        }
    }
}

void OverlayWindow_mac::ApplyClickThrough() {
    @autoreleasepool {
        if (!impl_->window) return;
        BOOL ignore = state_.IsClickThrough() ? YES : NO;
        [impl_->window setIgnoresMouseEvents:ignore];
        fprintf(stderr, "[Overlay-mac] Click-through: %s\n",
                ignore ? "ON" : "OFF");
    }
}

void OverlayWindow_mac::ApplyOpacity() {
    @autoreleasepool {
        if (!impl_->window) return;
        double alpha = static_cast<double>(state_.GetOpacity()) / 255.0;
        // Clamp to [0.0, 1.0] to be safe
        if (alpha < 0.0) alpha = 0.0;
        if (alpha > 1.0) alpha = 1.0;
        [impl_->window setAlphaValue:alpha];
        fprintf(stderr, "[Overlay-mac] Opacity: %.2f\n", alpha);
    }
}

bool OverlayWindow_mac::ApplyCaptureExclusion() {
    @autoreleasepool {
        if (!impl_->window) return false;
        // NSWindowSharingNone was already set in Create().
        // This method re-applies it in case the window was re-configured.
        [impl_->window setSharingType:NSWindowSharingNone];
        state_.SetCaptureExcluded(true);
        fprintf(stderr, "[Overlay-mac] Capture exclusion applied (NSWindowSharingNone)\n");
        return true;
    }
}

OsHandle OverlayWindow_mac::GetNativeHandle() const noexcept {
    return static_cast<OsHandle>(impl_->window);
}

bool OverlayWindow_mac::IsCreated() const noexcept {
    return impl_->window != nil;
}
