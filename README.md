# AI Overlay — Cross-Platform C++ Core

A native desktop overlay engine for Windows and macOS, designed as the C++ core of a larger AI assistant system. Renders a transparent, always-on-top, borderless window with global hotkeys, click-through toggling, opacity control, and a local WebSocket IPC server for integration with Flutter UI, Python audio/streaming engine, and Go backend.

---

## Features

| Feature | macOS | Windows |
|---------|-------|---------|
| Borderless transparent overlay | NSWindow / AppKit | WS_POPUP / WS_EX_LAYERED |
| Always on top (incl. fullscreen apps) | NSStatusWindowLevel + CanJoinAllSpaces | WS_EX_TOPMOST |
| Hidden from Dock / taskbar / switcher | NSApplicationActivationPolicyAccessory | WS_EX_TOOLWINDOW |
| Click-through toggle | `setIgnoresMouseEvents:` | WS_EX_TRANSPARENT |
| Opacity control | `setAlphaValue:` | SetLayeredWindowAttributes |
| Screen capture exclusion | NSWindowSharingNone | WDA_EXCLUDEFROMCAPTURE |
| Global hotkeys | CGEventTap (system-wide) | RegisterHotKey |
| WebSocket IPC server | ws://127.0.0.1:8765 | ws://127.0.0.1:8765 |
| Thread-safe UI dispatch | dispatch_async (main queue) | PostMessage |

---

## Architecture

```
┌──────────────────────────────────────────────────────────┐
│                    Shared (both platforms)                │
│  StateManager   MessageQueue<IPCCommand>   WebSocketServer│
└────────────────────────┬─────────────────────────────────┘
                         │
          ┌──────────────┴──────────────┐
          │                             │
┌─────────▼──────────┐       ┌──────────▼─────────┐
│       macOS         │       │      Windows        │
│  AppController_mac  │       │  AppController_win  │
│  OverlayWindow_mac  │       │  OverlayWindow_win  │
│  HotkeyManager_mac  │       │  HotkeyManager_win  │
│  (NSWindow/AppKit)  │       │  (HWND/Win32)       │
│  (CGEventTap)       │       │  (RegisterHotKey)   │
│  UI: NSRunLoop      │       │  UI: GetMessage loop│
│  Dispatch: GCD      │       │  Dispatch: PostMsg  │
└─────────────────────┘       └─────────────────────┘
```

The WebSocket server runs on its own thread and never touches any UI API directly. After pushing a command into the `IPCQueue`, it wakes the UI thread via the platform dispatcher. Neither platform ever blocks the UI thread.

---

## Project Structure

```
ai_overlay_cross/
├── CMakeLists.txt
├── vcpkg.json
├── include/
│   ├── Common.h                  # Platform detection, shared constants
│   ├── MessageQueue.h            # Thread-safe IPCQueue + command variants
│   ├── StateManager.h            # Mutex-protected runtime state
│   ├── IOverlayWindow.h          # Pure abstract overlay interface
│   ├── IHotkeyManager.h          # Pure abstract hotkey interface
│   ├── WebSocketServer.h         # Cross-platform WS server
│   ├── OverlayWindow_mac.h
│   ├── OverlayWindow_win.h
│   ├── HotkeyManager_mac.h
│   ├── HotkeyManager_win.h
│   ├── AppController_mac.h
│   └── AppController_win.h
└── src/
    ├── shared/
    │   ├── StateManager.cpp
    │   └── WebSocketServer.cpp   # websocketpp + nlohmann/json
    └── platform/
        ├── mac/                  # Objective-C++ (.mm)
        │   ├── main_mac.mm
        │   ├── AppController_mac.mm
        │   ├── OverlayWindow_mac.mm
        │   └── HotkeyManager_mac.mm
        └── win/                  # C++
            ├── main_win.cpp
            ├── AppController_win.cpp
            ├── OverlayWindow_win.cpp
            └── HotkeyManager_win.cpp
```

---

## Prerequisites

### macOS

| Tool | Minimum |
|------|---------|
| macOS | 11.0 Big Sur |
| Xcode Command Line Tools | 13+ |
| CMake | 3.21+ |
| Ninja | any |
| vcpkg | latest |

```bash
xcode-select --install
brew install cmake ninja
```

### Windows

| Tool | Minimum |
|------|---------|
| Windows | 10 version 2004 (build 19041) |
| Visual Studio | 2022 with Desktop C++ workload |
| CMake | 3.21+ |
| Windows SDK | 10.0.19041+ |
| vcpkg | latest |

---

## Building

### 1. Install vcpkg

**macOS**
```bash
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
echo 'export VCPKG_ROOT="$HOME/vcpkg"' >> ~/.zshrc
source ~/.zshrc
```

**Windows (PowerShell)**
```powershell
git clone https://github.com/microsoft/vcpkg.git C:\dev\vcpkg
C:\dev\vcpkg\bootstrap-vcpkg.bat
[System.Environment]::SetEnvironmentVariable("VCPKG_ROOT","C:\dev\vcpkg","User")
$env:VCPKG_ROOT = "C:\dev\vcpkg"
```

### 2. Install dependencies

**macOS — Apple Silicon**
```bash
$VCPKG_ROOT/vcpkg install websocketpp:arm64-osx asio:arm64-osx nlohmann-json:arm64-osx
```

**macOS — Intel**
```bash
$VCPKG_ROOT/vcpkg install websocketpp:x64-osx asio:x64-osx nlohmann-json:x64-osx
```

**Windows**
```powershell
& "$env:VCPKG_ROOT\vcpkg" install websocketpp:x64-windows asio:x64-windows nlohmann-json:x64-windows
& "$env:VCPKG_ROOT\vcpkg" integrate install
```

> Dependencies are also declared in `vcpkg.json` and will install automatically during CMake configure when the toolchain file is provided.

### 3. Configure

**macOS — Apple Silicon**
```bash
cmake -B build -S . -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -DCMAKE_OSX_ARCHITECTURES=arm64
```

**macOS — Intel**
```bash
cmake -B build -S . -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-osx \
  -DCMAKE_OSX_ARCHITECTURES=x86_64
```

**Windows (Developer PowerShell)**
```powershell
cmake -B build -S . `
  -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows
```

### 4. Build

**macOS**
```bash
cmake --build build --config Release --parallel $(sysctl -n hw.ncpu)
# Output: build/ai_overlay
```

**Windows**
```powershell
cmake --build build --config Release --parallel
# Output: build\Release\ai_overlay.exe
```

---

## Running

**macOS**
```bash
./build/ai_overlay
```

**Windows**
```powershell
.\build\Release\ai_overlay.exe
```

> **macOS — Accessibility permission required for global hotkeys.**
> On first launch, grant access via **System Settings → Privacy & Security → Accessibility**. The overlay and IPC work without it; only hotkeys are affected.

> **Windows — Firewall prompt.** The WebSocket server binds to `127.0.0.1:8765` (loopback only). Allow it on private networks.

---

## Hotkeys

| Hotkey | Action |
|--------|--------|
| `Ctrl + Shift + X` | Toggle click-through |
| `Ctrl + Shift + Z` | Toggle opacity (low ↔ high) |

---

## IPC — WebSocket API

Connect to `ws://127.0.0.1:8765` from any WebSocket client (Flutter, Python, Go, or CLI).

**Toggle click-through**
```json
{ "type": "toggle_clickthrough" }
```

**Set opacity** (0–255)
```json
{ "type": "set_opacity", "value": 120 }
```

All commands return `{"status":"ok"}` on success or `{"status":"error","message":"..."}` on failure.

### Quick test with wscat
```bash
npm install -g wscat
wscat -c ws://127.0.0.1:8765
> { "type": "set_opacity", "value": 180 }
< {"status":"ok"}
```

---

## Screen Capture Exclusion

| Platform | Method | Requirement |
|----------|--------|-------------|
| macOS | `NSWindowSharingNone` | None — works on all supported macOS versions |
| Windows | `WDA_EXCLUDEFROMCAPTURE` | Win10 2004 (build 19041)+; falls back to `WDA_MONITOR` on older builds |

---

## Dependencies

| Library | Purpose | License |
|---------|---------|---------|
| [websocketpp](https://github.com/zaphoyd/websocketpp) | WebSocket server | BSD |
| [asio](https://think-async.com/Asio/) | Async I/O (standalone, no Boost) | BSL-1.0 |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON parsing | MIT |

System frameworks (no installation required): AppKit, Carbon, CoreFoundation, ApplicationServices (macOS) · user32, gdi32, dwmapi, ws2_32 (Windows).

---

## Debug Output

**macOS** — logs to `stderr`, visible in Terminal or Console.app.

**Windows** — logs to `OutputDebugString`. View with [DebugView](https://learn.microsoft.com/sysinternals/downloads/debugview) or the Visual Studio Output window.

---

## License

MIT
