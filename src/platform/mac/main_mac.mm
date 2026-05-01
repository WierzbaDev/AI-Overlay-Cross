// main_mac.mm
// Objective-C++ entry point for macOS.

#import <Cocoa/Cocoa.h>
#include <cstdio>

#include "AppController_mac.h"

int main(int argc, const char* argv[]) {
    @autoreleasepool {
        (void)argc;
        (void)argv;

        AppController_mac app;

        if (!app.Init()) {
            fprintf(stderr, "[main-mac] Fatal: Init() failed\n");
            return 1;
        }

        return app.Run();
        // app destructor calls Shutdown()
    }
}
