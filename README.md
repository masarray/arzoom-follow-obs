# ArZoom — Smart Mouse Zoom for OBS

ArZoom is a native OBS video filter for screen tutorials, software demonstrations, engineering training, live classes, and livestream presentations.

Press one OBS hotkey to zoom in. ArZoom follows the mouse smoothly without exposing black edges. Press the same hotkey again to glide back to the normal full-screen view.

## Product goal

ArZoom is intentionally focused:

- easy for first-time OBS users
- one toggle hotkey
- smooth, comfortable movement
- Smart Follow that does not shake on every small mouse movement
- hard edge protection
- one-pass GPU rendering
- near-zero idle overhead
- fail-safe behavior for recording and livestreaming

## MVP support

- Windows 10/11 x64
- OBS Studio Display Capture
- OBS 31.x build target
- expected forward testing on OBS 32.x
- Zoom 1.10× to 4.00×
- Smart, Centered, and Fixed follow modes

## How to use

1. Install ArZoom and restart OBS.
2. Add or select a **Display Capture** source.
3. Open **Filters**.
4. Add **ArZoom — Smart Mouse Zoom**.
5. Keep the default values:
   - Zoom amount: `2.00×`
   - Mouse follow: `Smart`
   - Movement: `Smooth`
   - Stable safe zone: `28%`
6. Open **OBS Settings → Hotkeys**.
7. Find **ArZoom — Toggle Zoom & Mouse Follow**. The row is registered globally when OBS loads the plugin, so it is visible even before a filter is added.
8. Assign a shortcut, for example `Ctrl + ~`.

Press once to zoom and follow. Press again to return smoothly to normal.

## Why Smart Follow is comfortable

ArZoom does not move the camera for every tiny mouse movement. The cursor can move inside a stable safe zone while the viewport remains still. The viewport moves only when the cursor leaves that zone, and only enough to bring it back to the nearest boundary.

This keeps tutorials readable and reduces motion sickness.

## Edge safety

The viewport center is clamped mathematically according to the active zoom level. It cannot sample outside the captured screen, including near corners and monitor edges.

The shader also applies a final UV clamp as a numerical safety guard.

## Performance design

When zoom is inactive and the animation has settled, ArZoom calls OBS pass-through and does not run its transform shader.

While active:

- cursor position is sampled once per video tick
- monitor enumeration is cached
- motion calculations are constant-time
- rendering is a single GPU texture sample
- no frame readback
- no image copy through the CPU
- no file I/O
- no per-frame OBS setting writes

## Build on Windows

Requirements:

- Windows 10/11 x64
- Git
- CMake 3.28+
- Visual Studio 2022 or 2026
- **Desktop development with C++**

Double-click:

```text
build-local-windows.bat
```

On the first run, the script downloads and prepares OBS build dependencies. Later runs reuse the cached `.build/` workspace and skip the expensive configure step.

If compilation already succeeded but packaging failed, run:

```text
package-existing-build.bat
```

This reuses `release\stage\RelWithDebInfo` and creates the package without downloading or rebuilding OBS.

Install Inno Setup 6 if you also want the `.exe` installer.

## Test the motion core

The motion and edge math is independent from OBS:

```bash
g++ -std=c++17 -O2 tests/arzoom-math-test.cpp -o arzoom-math-test
./arzoom-math-test
```

The randomized test checks 200,000 zoom/center combinations and verifies that the visible viewport never leaves valid source coordinates.

## Repository layout

```text
src/
  plugin-main.cpp
  arzoom-filter.cpp
  arzoom-math.hpp
data/
  effects/arzoom.effect
  locale/en-US.ini
  locale/id-ID.ini
scripts/
packaging/windows/
tests/
```

## Current validation status

Implemented and statically validated:

- toggle state
- hotkey anti-repeat
- frame-rate-independent zoom
- Smart Follow dead zone
- centered and fixed modes
- edge clamping
- smooth zoom-out to center
- multi-monitor coordinates, including negative desktop coordinates
- auto monitor detection for OBS `monitor` and `monitor_id` settings
- fail-safe pass-through
- idle bypass
- Windows build and packaging scripts
- English and Indonesian locale

Still requires real OBS runtime validation before calling v1.0 stable:

- OBS 31 and OBS 32 loading
- 1080p60 streaming + recording
- 1440p60 and 4K
- mixed-DPI dual monitors
- NVIDIA, AMD, and Intel GPUs
- fast repeated hotkey switching

## License

GPL-2.0-or-later.

## Hotkey setup

From the ArZoom filter panel, click **Open OBS Hotkeys Settings**. OBS opens directly on the Hotkeys page. Assign a shortcut to `ArZoom — Toggle Zoom & Mouse Follow`, then click Apply or OK. ArZoom explicitly restores the saved binding when OBS restarts and when the active profile changes.
