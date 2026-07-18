# ArZoom for OBS

<p align="center">
  <strong>Smooth mouse-follow zoom for OBS screen capture.</strong><br>
  Press one hotkey to zoom in and follow the cursor. Press it again to glide back to the full screen.
</p>

<p align="center">
  <a href="https://github.com/masarray/arzoom-follow-obs/releases"><img alt="Download ArZoom" src="https://img.shields.io/badge/Download-Windows%20installer-2563eb?style=for-the-badge&logo=windows11&logoColor=white"></a>
  <a href="https://masarray.github.io/arzoom-follow-obs/"><img alt="ArZoom website" src="https://img.shields.io/badge/Open-Beginner%20guide-0f766e?style=for-the-badge"></a>
</p>

<p align="center">
  <img alt="Version 0.1.4 preview" src="https://img.shields.io/badge/version-0.1.4%20preview-f59e0b">
  <img alt="Windows 10 and 11" src="https://img.shields.io/badge/Windows-10%20%7C%2011-0078d4?logo=windows11&logoColor=white">
  <img alt="OBS Studio plugin" src="https://img.shields.io/badge/OBS%20Studio-native%20plugin-302e31?logo=obsstudio&logoColor=white">
  <a href="LICENSE"><img alt="GPL 2.0 or later" src="https://img.shields.io/badge/license-GPL--2.0--or--later-blue"></a>
</p>

ArZoom is a native **OBS zoom plugin** for tutorials, software demonstrations, engineering training, online classes, product walkthroughs, and live streaming. It enlarges a Display Capture source in real time, follows the mouse smoothly, and prevents the viewport from exposing black edges near the sides or corners of the screen.

> **Current status:** Windows preview release. The core motion, edge-safety, hotkey persistence, packaging, and fail-safe paths are implemented. Broader hardware and OBS runtime validation is still in progress before v1.0.

## What it feels like

```text
Press your ArZoom hotkey
        ↓
Smooth zoom-in
        ↓
Smart Follow keeps the cursor comfortably visible
        ↓
Move to any screen edge without revealing black borders
        ↓
Press the same hotkey again
        ↓
Smooth recenter and zoom-out to the full screen
```

## Why viewers stay comfortable

ArZoom is designed to make screen recordings easier to watch—not to move the frame every time the mouse moves by one pixel.

- **Smart Follow:** the viewport stays still while the cursor moves inside a stable safe zone.
- **Smooth motion:** zoom and pan use frame-rate-independent animation.
- **Edge protection:** the visible area is mathematically clamped inside the captured monitor.
- **One-key control:** the same OBS hotkey zooms in and returns to normal.
- **GPU rendering:** active zoom uses a single-pass video effect with no CPU frame readback.
- **Idle bypass:** when zoom is inactive, ArZoom returns the original frame through OBS pass-through.
- **Fail-safe behavior:** a shader or monitor-mapping problem keeps the source visible instead of producing a black frame.
- **No telemetry:** ArZoom does not send analytics, cursor data, or captured video anywhere.

## Install in 5 minutes

### 1. Download

Open [GitHub Releases](https://github.com/masarray/arzoom-follow-obs/releases) and download the newest Windows installer.

### 2. Install

Close OBS completely, run the installer, then start OBS again.

### 3. Add the filter

```text
Display Capture
→ Filters
→ Effect Filters
→ +
→ ArZoom - Smart Mouse Zoom
```

### 4. Assign the hotkey

Inside the ArZoom filter, click **Open OBS Hotkeys Settings**. Assign a shortcut to:

```text
ArZoom — Toggle Zoom & Mouse Follow
```

A practical example is `Ctrl + ~`, although any non-conflicting shortcut is fine. Click **Apply** or **OK**. ArZoom restores the saved binding after OBS restarts and when the active OBS profile changes.

### 5. Start with the recommended settings

| Setting | Recommended value | Purpose |
|---|---:|---|
| Zoom amount | `2.00×` | Clear detail without excessive motion |
| Mouse follow | `Smart` | Comfortable tracking for tutorials |
| Movement | `Smooth` | Gentle motion for viewers |
| Stable safe zone | `28%` | Prevents constant micro-panning |
| Target monitor | `Auto` | Reads the monitor used by Display Capture |

Press the hotkey once to zoom and follow the mouse. Press it again to return smoothly to the normal full-screen view.

## Best use cases

- OBS screen zoom for software tutorials
- coding and engineering demonstrations
- live product walkthroughs
- online teaching and webinars
- spreadsheet and dashboard explanations
- control-system, CAD, SCADA, HMI, and technical application training
- livestream presentations where small interface elements must remain readable

## Compatibility

| Item | Current support |
|---|---|
| Operating system | Windows 10/11 x64 |
| OBS source | Display Capture |
| Build target | OBS Studio 31.1.1 |
| OBS 32.x | Forward validation in progress |
| Zoom range | `1.10×` to `4.00×` |
| Follow modes | Smart, Centered, Fixed |
| Multi-monitor | Supported, including negative desktop coordinates |
| Mixed DPI | Implemented; broader physical-device validation in progress |
| macOS / Linux | Not available in the current preview |
| Window Capture / Game Capture | Not part of the current MVP |

## Common questions

### The filter does not appear

Close OBS completely and reinstall the full package. Confirm that the OBS installation directory contains both the ArZoom plugin DLL and its `data` folder.

### The ArZoom hotkey row does not appear

Confirm the ArZoom filter controls are visible, restart OBS, then search `ArZoom` in **Settings → Hotkeys**. The current plugin registers one global frontend hotkey.

### The shortcut disappears after restart

Use v0.1.4 or newer. Assign the shortcut, click **Apply** or **OK**, then use **Save current hotkey now** in the filter panel if required.

### Zoom works, but the mouse is not followed

Set **Mouse follow** to `Smart` or `Centered`, keep **Target monitor** on `Auto`, and verify the filter is attached to the intended Display Capture source.

### The viewport stops moving near the screen edge

That is expected. ArZoom stops the viewport at the last valid position so it never reveals pixels outside the captured display.

More help is available in the [beginner guide](https://masarray.github.io/arzoom-follow-obs/guide.html) and [troubleshooting page](https://masarray.github.io/arzoom-follow-obs/troubleshooting.html).

## Performance architecture

During each active video tick, ArZoom samples the cursor once, maps it into the captured monitor, calculates the Smart Follow target, clamps the target to valid screen bounds, and updates the animation state. Rendering then applies the calculated zoom and center through one GPU effect pass.

ArZoom does **not** perform frame readback, OCR, image analysis, per-frame file I/O, or per-frame OBS settings writes.

## Build from source

Requirements:

- Windows 10/11 x64
- Git
- CMake 3.28+
- Visual Studio 2022 or 2026
- **Desktop development with C++**
- Inno Setup 6 for the optional installer build

Run:

```text
build-local-windows.bat
```

The first build downloads and prepares the OBS development dependencies. Later builds reuse the `.build/` cache. To package an already successful build without recompiling, run:

```text
package-existing-build.bat
```

## Validate the motion core

The motion and edge calculations can be tested independently from OBS:

```bash
g++ -std=c++17 -O2 tests/arzoom-math-test.cpp -o arzoom-math-test
./arzoom-math-test
```

The deterministic test checks 200,000 zoom and viewport combinations and verifies that the visible area stays inside valid source coordinates.

## Project documentation

- [Getting started](https://masarray.github.io/arzoom-follow-obs/guide.html)
- [Troubleshooting](https://masarray.github.io/arzoom-follow-obs/troubleshooting.html)
- [Support policy](SUPPORT.md)
- [Contributing](CONTRIBUTING.md)
- [Security policy](SECURITY.md)
- [Changelog](CHANGELOG.md)

## Contributing

Bug reports, hardware compatibility results, documentation fixes, translations, and focused pull requests are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) before opening a contribution.

## License

ArZoom is open-source software licensed under [GPL-2.0-or-later](LICENSE).

OBS Studio is a trademark of its respective owners. ArZoom is an independent community project and is not affiliated with or endorsed by the OBS Project.
