// AppController_mac.mm
// Objective-C++ — compiled only on macOS.

#import <Cocoa/Cocoa.h>
#include <cstdio>
#include <memory>
#include <vector>

#include "AppController_mac.h"
#include "StateManager.h"
#include "MessageQueue.h"
#include "OverlayWindow_mac.h"
#include "HotkeyManager_mac.h"
#include "WebSocketServer.h"

// ============================================================================
// Pimpl — defined FIRST so @interface can reference it by pointer.
// The @interface stores it as void* anyway to keep the header pure-C++,
// but we need the complete type here in the .mm translation unit.
// ============================================================================
struct AppController_mac::Impl {
    StateManager                       state;
    std::unique_ptr<IPCQueue>          ipcQueue;
    std::unique_ptr<OverlayWindow_mac> overlay;
    std::unique_ptr<HotkeyManager_mac> hotkeys;
    std::unique_ptr<WebSocketServer>   wsServer;
    bool                               initialised = false;
    bool                               shutdown    = false;

    // Called on the main thread to drain and apply all queued IPC commands.
    void DrainIPCQueue() {
        std::vector<IPCCommand> cmds;
        cmds.reserve(8);
        ipcQueue->TryDrainAll(cmds);

        for (auto& cmd : cmds) {
            std::visit([this](auto&& c) {
                using T = std::decay_t<decltype(c)>;

                if constexpr (std::is_same_v<T, CmdToggleClickthrough>) {
                    state.ToggleClickThrough();
                    if (overlay) overlay->ApplyClickThrough();
                }
                else if constexpr (std::is_same_v<T, CmdSetOpacity>) {
                    state.SetOpacity(c.value);
                    if (overlay) overlay->ApplyOpacity();
                }
            }, cmd);
        }
    }

    void DoShutdown() {
        if (shutdown) return;
        shutdown = true;

        if (wsServer) { wsServer->Stop();           wsServer.reset(); }
        if (ipcQueue) { ipcQueue->Shutdown();        ipcQueue.reset(); }
        if (hotkeys)  { hotkeys->UnregisterAll();    hotkeys.reset();  }
        if (overlay)  { overlay->Destroy();          overlay.reset();  }

        fprintf(stderr, "[App-mac] Shutdown complete\n");
    }
};

// ============================================================================
// AIAppDelegate
// The impl pointer is stored as void* so the header (AppController_mac.h)
// remains pure C++ and does not need to see the Impl definition.
// Inside this .mm file we have the complete type and can cast freely.
// ============================================================================
@interface AIAppDelegate : NSObject <NSApplicationDelegate>
// void* to keep ObjC interface independent of the C++ Impl type
@property (nonatomic, assign) void* implPtr;
@end

@implementation AIAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    (void)notification;
    // Nothing extra — Init() ran before [NSApp run]
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    (void)sender;
    // The overlay is not a regular document window — do not quit on close
    return NO;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    (void)sender;
    // Run cleanup synchronously before the run loop exits
    if (self.implPtr) {
        auto* impl = static_cast<AppController_mac::Impl*>(self.implPtr);
        impl->DoShutdown();
    }
    return NSTerminateNow;
}

@end

// ============================================================================
// AppController_mac
// ============================================================================

AppController_mac::AppController_mac()
    : impl_(std::make_unique<Impl>())
{}

AppController_mac::~AppController_mac() {
    Shutdown();
}

// ---------------------------------------------------------------------------
// Init
// ---------------------------------------------------------------------------
bool AppController_mac::Init() {
    assert(!impl_->initialised && "Init() called twice");

    @autoreleasepool {
        // ------------------------------------------------------------------
        // 1. NSApplication — must be done before any UI work
        // ------------------------------------------------------------------
        [NSApplication sharedApplication];

        // NSApplicationActivationPolicyAccessory:
        //   - no Dock icon
        //   - excluded from Cmd+Tab switcher
        //   - app stays alive with no visible windows
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];

        AIAppDelegate* delegate = [[AIAppDelegate alloc] init];
        delegate.implPtr = static_cast<void*>(impl_.get());
        [NSApp setDelegate:delegate];

        // Keep a strong reference to the delegate through impl_
        // (ObjC delegates are weak by convention; NSApp holds a weak ref)
        // We store it in a raw __strong ivar via a small wrapper:
        // Simplest approach: just retain it explicitly.
        // ARC retains objects assigned to strong properties automatically,
        // and NSApp holds a weak ref. We need our own strong ref.
        // Solution: store in a local static is wrong for multiple instances,
        // so we use CFBridgingRetain / __bridge_retained pattern.
        // Actually the cleanest: store the delegate in the Impl struct as void*
        // and CFRetain it. Let's do that:
        impl_->wsServer = nullptr; // placeholder; set below after window created

        // Store delegate strongly — ARC handles release when impl_ is destroyed
        // via a simple trick: store in a __strong local and bridge-retain.
        // We use a global static for simplicity since this is a single-instance app.
        static __strong AIAppDelegate* s_delegate = nil;
        s_delegate = delegate;

        // ------------------------------------------------------------------
        // 2. IPC queue
        // ------------------------------------------------------------------
        impl_->ipcQueue = std::make_unique<IPCQueue>();

        // ------------------------------------------------------------------
        // 3. Overlay window — full primary screen coverage
        // ------------------------------------------------------------------
        impl_->overlay = std::make_unique<OverlayWindow_mac>(impl_->state);

        NSScreen* screen = [NSScreen mainScreen];
        NSRect    frame  = [screen frame];

        int x = static_cast<int>(frame.origin.x);
        int y = static_cast<int>(frame.origin.y);
        int w = static_cast<int>(frame.size.width);
        int h = static_cast<int>(frame.size.height);

        if (!impl_->overlay->Create(x, y, w, h)) {
            fprintf(stderr, "[App-mac] Fatal: Failed to create overlay window\n");
            return false;
        }

        // ------------------------------------------------------------------
        // 4. Global hotkeys via CGEventTap
        // ------------------------------------------------------------------
        impl_->hotkeys = std::make_unique<HotkeyManager_mac>();

        // Capture impl_ raw pointer — safe because:
        // (a) hotkeys is destroyed in DoShutdown() before impl_ is freed
        // (b) the callback is dispatched to main queue, which is serialized
        //     with DoShutdown() (also runs on main queue)
        AppController_mac::Impl* rawImpl = impl_.get();

        HotkeyCallback hotkeyCb = [rawImpl](HotkeyAction action) {
            // Already on main thread (dispatched in HotkeyManager_mac)
            switch (action) {
                case HotkeyAction::ToggleClickThrough:
                    rawImpl->state.ToggleClickThrough();
                    if (rawImpl->overlay) rawImpl->overlay->ApplyClickThrough();
                    break;
                case HotkeyAction::ToggleOpacity:
                    rawImpl->state.ToggleOpacity();
                    if (rawImpl->overlay) rawImpl->overlay->ApplyOpacity();
                    break;
            }
        };

        if (!impl_->hotkeys->RegisterAll(nullptr, std::move(hotkeyCb))) {
            // Non-fatal: log and continue without global hotkeys
            fprintf(stderr,
                "[App-mac] Warning: Hotkey registration failed. "
                "Grant Accessibility access in System Preferences.\n");
        }

        // ------------------------------------------------------------------
        // 5. WebSocket server
        //    UI notifier uses dispatch_async to main queue — never blocks WS thread
        // ------------------------------------------------------------------
        AppController_mac::Impl* notifyImpl = impl_.get();

        WebSocketServer::UINotifier notifier = [notifyImpl]() {
            // Called from WS thread — dispatch to main queue (UI thread)
            dispatch_async(dispatch_get_main_queue(), ^{
                if (!notifyImpl->shutdown) {
                    notifyImpl->DrainIPCQueue();
                }
            });
        };

        impl_->wsServer = std::make_unique<WebSocketServer>(
            *impl_->ipcQueue, std::move(notifier));
        impl_->wsServer->port = WS_SERVER_PORT;

        if (!impl_->wsServer->Start()) {
            // Non-fatal: overlay and hotkeys still work
            fprintf(stderr,
                "[App-mac] Warning: WebSocket server failed to start on port %u\n",
                static_cast<unsigned>(WS_SERVER_PORT));
        }

        // ------------------------------------------------------------------
        // 6. Show the overlay
        // ------------------------------------------------------------------
        impl_->overlay->Show();

        impl_->initialised = true;
        fprintf(stderr, "[App-mac] Initialisation complete\n");
        return true;
    }
}

// ---------------------------------------------------------------------------
// Run
// ---------------------------------------------------------------------------
int AppController_mac::Run() {
    assert(impl_->initialised && "Run() called before Init()");

    @autoreleasepool {
        // Activate without stealing focus from the frontmost application
        [NSApp activateIgnoringOtherApps:NO];
        // Blocks until [NSApp terminate:] is called (from applicationShouldTerminate:)
        [NSApp run];
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------
void AppController_mac::Shutdown() {
    impl_->DoShutdown();
}
