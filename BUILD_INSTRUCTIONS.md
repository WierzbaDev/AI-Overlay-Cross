# AI Overlay Cross-Platform — Build Instructions

## Prerequisites

### macOS

| Tool | Minimum | Notes |
|------|---------|-------|
| macOS | 11.0 (Big Sur) | Required for full NSWindowSharingNone support |
| Xcode Command Line Tools | 13+ | `xcode-select --install` |
| CMake | 3.21+ | `brew install cmake` |
| Ninja | any recent | `brew install ninja` |
| vcpkg | latest | See below |
| Homebrew | any | https://brew.sh |

### Windows

| Tool | Minimum | Notes |
|------|---------|-------|
| Windows | 10 version 2004 (build 19041) | Required for WDA_EXCLUDEFROMCAPTURE |
| Visual Studio | 2022 | Desktop C++ workload |
| CMake | 3.21+ | Bundled with VS2022 or cmake.org |
| vcpkg | latest | See below |
| Windows SDK | 10.0.19041+ | Bundled with VS2022 |

---

## Step 1 — Install vcpkg

### macOS
```bash
cd ~
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh

# Add to your shell profile (~/.zshrc or ~/.bashrc):
export VCPKG_ROOT="$HOME/vcpkg"
export PATH="$VCPKG_ROOT:$PATH"
```

### Windows (PowerShell)
```powershell
cd C:\dev
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# Set environment variable (persisted):
[System.Environment]::SetEnvironmentVariable("VCPKG_ROOT","C:\dev\vcpkg","User")
$env:VCPKG_ROOT = "C:\dev\vcpkg"
```

---

## Step 2 — Install Dependencies

### macOS
```bash
cd $VCPKG_ROOT
./vcpkg install websocketpp:arm64-osx   # Apple Silicon
./vcpkg install asio:arm64-osx
./vcpkg install nlohmann-json:arm64-osx

# OR for Intel Macs:
./vcpkg install websocketpp:x64-osx
./vcpkg install asio:x64-osx
./vcpkg install nlohmann-json:x64-osx
```

> **Note:** vcpkg manifest mode (vcpkg.json already in repo) installs
> everything automatically during CMake configure if you pass the toolchain file.
>
> If your local `vcpkg` registry does not contain `websocketpp`, install the
> Homebrew fallback dependencies instead:
>
> ```bash
> brew install websocketpp boost nlohmann-json ninja
> ```

### Windows (PowerShell)
```powershell
cd $env:VCPKG_ROOT
.\vcpkg install websocketpp:x64-windows
.\vcpkg install asio:x64-windows
.\vcpkg install nlohmann-json:x64-windows
.\vcpkg integrate install
```

---

## Step 3 — Configure with CMake

### macOS (Apple Silicon)
```bash
cd /path/to/ai_overlay_cross

cmake -B build -S . \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -DCMAKE_OSX_ARCHITECTURES=arm64
```

### macOS (Intel)
```bash
cmake -B build -S . \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=x64-osx \
  -DCMAKE_OSX_ARCHITECTURES=x86_64
```

### macOS (Universal Binary — Apple Silicon + Intel)
```bash
cmake -B build -S . \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
```

### macOS (Homebrew-only fallback)
```bash
brew install websocketpp boost nlohmann-json ninja

cmake -B build -S . \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64
```

### Windows (Developer PowerShell for VS 2022)
```powershell
cd path\to\ai_overlay_cross

cmake -B build -S . `
  -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DCMAKE_BUILD_TYPE=Release
```

---

## Step 4 — Build

### macOS
```bash
cmake --build build --config Release --parallel $(sysctl -n hw.ncpu)
```

Output: `build/ai_overlay`

### Windows
```powershell
cmake --build build --config Release --parallel
```

Output: `build\Release\ai_overlay.exe`

---

## Step 5 — macOS Accessibility Permission

The CGEventTap global hotkey system requires **Accessibility** permission.

On first launch, macOS will show a permission dialog. If it does not:

1. Open **System Preferences** → **Privacy & Security** → **Accessibility**
2. Click the lock icon and authenticate
3. Add `ai_overlay` to the list and enable it
4. Restart `ai_overlay`

Without this permission, global hotkeys are silently disabled. The overlay
and IPC still function normally.

---

## Step 6 — Run

### macOS
```bash
./build/ai_overlay
```

### Windows
```powershell
.\build\Release\ai_overlay.exe
```

A Windows Firewall prompt may appear for the WebSocket server binding to
`127.0.0.1:8765` (loopback only — safe to allow on private networks).

---

## Testing IPC

Install `wscat`:
```bash
npm install -g wscat     # macOS
npm install -g wscat     # Windows (requires Node.js)
```

Connect:
```bash
wscat -c ws://127.0.0.1:8765
```

Send commands:
```json
{ "type": "toggle_clickthrough" }
```
```json
{ "type": "set_opacity", "value": 120 }
```
```json
{ "type": "set_opacity", "value": 220 }
```

Server responds `{"status":"ok"}` or `{"status":"error","message":"..."}`.

---

## Hotkeys (both platforms)

| Hotkey | Action |
|--------|--------|
| `Ctrl + Shift + X` | Toggle click-through (input transparency) |
| `Ctrl + Shift + Z` | Toggle opacity (low ~31% ↔ high ~86%) |

---

## Debug Output

### macOS
All logs go to `stderr`. View in Terminal or Console.app.

### Windows
All logs go to `OutputDebugString`. View with:
- **DebugView** (Sysinternals): https://learn.microsoft.com/sysinternals/downloads/debugview
- Visual Studio **Output** window when running under debugger

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
│                     │       │                     │
│  AppController_mac  │       │  AppController_win  │
│  OverlayWindow_mac  │       │  OverlayWindow_win  │
│  HotkeyManager_mac  │       │  HotkeyManager_win  │
│  (NSWindow/AppKit)  │       │  (HWND/Win32)       │
│  (CGEventTap)       │       │  (RegisterHotKey)   │
│                     │       │                     │
│  UI thread:         │       │  UI thread:         │
│  NSRunLoop          │       │  GetMessage loop    │
│                     │       │                     │
│  Dispatch to UI:    │       │  Dispatch to UI:    │
│  dispatch_async     │       │  PostMessage        │
│  (main queue)       │       │  WM_APP_IPC_MESSAGE │
└─────────────────────┘       └─────────────────────┘
```

### Threading guarantee
The WebSocket server runs on its own thread. It **never** calls any UI API
directly. After pushing a command into the lock-free `IPCQueue`, it triggers
the platform dispatcher:
- macOS: `dispatch_async(dispatch_get_main_queue(), ...)` — guaranteed UI thread
- Windows: `PostMessage(hwnd, WM_APP_IPC_MESSAGE, ...)` — posted to message queue,
  processed by the UI thread on its next `GetMessage` iteration

Neither platform ever blocks the UI thread.

---

## Capture Exclusion Details

| Platform | Method | Notes |
|----------|--------|-------|
| macOS | `NSWindowSharingNone` | Hides from all screen capture APIs including ScreenCaptureKit |
| Windows | `WDA_EXCLUDEFROMCAPTURE` (0x11) | Requires Win10 2004+; falls back to `WDA_MONITOR` on older builds |

On macOS, `NSWindowSharingNone` is reliable and requires no special permissions.
On Windows, `WDA_EXCLUDEFROMCAPTURE` requires Win10 2004 (build 19041) or later.
The code detects the OS version at runtime and falls back gracefully.
