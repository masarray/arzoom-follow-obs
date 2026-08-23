# ArZoom for OBS

<p align="center">
  <strong>Intent-driven Smart Camera zoom & follow for OBS screen capture.</strong><br>
  Zoom where you are presenting, ignore distracting mouse jitter, and move the frame with controlled cinematic motion.
</p>

<p align="center">
  <a href="https://github.com/masarray/arzoom-follow-obs/releases/latest/download/ArZoom-OBS-Setup-windows-x64.exe"><img alt="Download ArZoom installer" src="https://img.shields.io/badge/Download-Windows%20installer-2563eb?style=for-the-badge&logo=windows11&logoColor=white"></a>
  <a href="https://masarray.github.io/arzoom-follow-obs/"><img alt="ArZoom website" src="https://img.shields.io/badge/Open-Beginner%20guide-0f766e?style=for-the-badge"></a>
</p>

<p align="center">
  <img alt="Version 0.2.0 public trial" src="https://img.shields.io/badge/version-0.2.0%20public%20trial-f59e0b">
  <img alt="Windows 10 and 11" src="https://img.shields.io/badge/Windows-10%20%7C%2011-0078d4?logo=windows11&logoColor=white">
  <img alt="OBS Studio plugin" src="https://img.shields.io/badge/OBS%20Studio-native%20plugin-302e31?logo=obsstudio&logoColor=white">
  <a href="LICENSE"><img alt="GPL 2.0 or later" src="https://img.shields.io/badge/license-GPL--2.0--or--later-blue"></a>
</p>

ArZoom is a native **OBS presentation-camera plugin** for tutorials, engineering training, software demonstrations, online classes, product walkthroughs, and live streaming. It applies zoom/pan to a Display Capture source through a GPU filter while the Smart Camera interprets cursor movement as **presenter intent**, not as a command to mechanically chase every mouse movement.

> **Current status:** v0.2.0 public trial for Windows. Smart Camera Motion 2.0, edge safety, global hotkey persistence, standard/portable-aware installation, deterministic motion tests, and fail-safe rendering are implemented. Broader hardware and OBS-version validation continues before v1.0.

## What changed in Smart Camera Motion 2.0

The v0.1.x motion model behaved like a stable edge follower. v0.2.0 replaces that runtime with an intent-driven camera model.

- **Focus-preserving activation:** if the pointer is near an edge/corner when zoom begins, ArZoom moves toward that focus as the zoom opens legal pan room. It no longer intentionally zooms into unrelated center content first and only then searches for the pointer.
- **Viewer-comfort filtering:** normal hand jitter and small explanatory cursor gestures inside the comfort region do not continuously move the frame.
- **Intent observation:** a possible relocation is observed briefly before the camera commits, reducing robotic reactions to incidental pointer motion.
- **Ballistic movement:** persistent velocity and acceleration produce controlled launch, travel, braking, and settle instead of position-only smoothing.
- **Adaptive catch-up:** large relocation and output-edge risk increase urgency without bypassing jerk/edge limits.
- **Settle lock:** once useful framing is reached, residual micro-motion is stopped.
- **Restrained look-ahead:** sustained cursor travel can bias framing slightly forward without turning the camera into a sticky cursor follower.

The runtime states are conceptually:

```text
REST → OBSERVE → FOLLOW → CATCH-UP → BRAKE → SETTLE
  ↘             focus-preserving activation            ↗
```

## Install — normal OBS or OBS Portable

### 1. Download the installer

[Download the latest ArZoom Windows installer](https://github.com/masarray/arzoom-follow-obs/releases/latest/download/ArZoom-OBS-Setup-windows-x64.exe).

Close OBS completely before installation.

### 2. Choose the correct installation mode

The same EXE supports both:

**Standard OBS Studio**

- choose **Standard OBS Studio**;
- the installer auto-detects common OBS installation locations;
- the destination is validated before files are copied.

**OBS Portable / custom OBS folder**

- choose **OBS Portable / custom OBS folder**;
- browse to the **OBS root folder** you actually launch;
- a valid root contains:

```text
bin\64bit\obs64.exe
```

For example:

```text
D:\PortableApps\OBS\
├─ bin\64bit\obs64.exe
├─ obs-plugins\
└─ data\
```

Select `D:\PortableApps\OBS\`, **not** `bin\64bit` and not `obs-plugins`.

The installer remembers the last valid custom OBS root for future upgrades.

### Manual ZIP

A manual ZIP remains available in Releases. Merge its `obs-plugins` and `data` folders into the OBS root folder. This is useful for users who prefer direct extraction or maintain multiple portable OBS copies.

## Add ArZoom to OBS

```text
Display Capture
→ Filters
→ Effect Filters
→ +
→ ArZoom - Smart Camera Zoom & Follow
```

Then open **OBS Settings → Hotkeys** and assign a shortcut to:

```text
ArZoom — Toggle Smart Camera Zoom
```

The filter also includes **Open OBS Hotkeys Settings** and explicit hotkey persistence.

## Recommended v0.2.0 settings

| Setting | Recommended | Purpose |
|---|---:|---|
| Zoom amount | `2.00×` | Useful detail without excessive crop |
| Camera follow | `Smart Camera` | Intent-driven presentation tracking |
| Camera character | `Balanced` | Recommended launch/catch-up/brake balance |
| Stable comfort zone | `28%` | Lets normal explanatory pointer motion happen without camera shake |
| Target monitor | `Auto` | Maps the Display Capture monitor automatically |

Camera character options:

- **Cinematic** — strongest stability and gentlest motion.
- **Balanced** — recommended default.
- **Responsive** — faster catch-up while retaining ballistic movement.

## Why ArZoom stays lightweight

Smart Camera intelligence is deterministic vector math; it does not require AI, OCR, computer vision, or frame analysis.

The hot path is designed around:

- one cursor sample per active video tick;
- fixed-size camera state;
- no frame readback;
- no image analysis;
- no per-frame file I/O;
- no per-frame OBS settings writes;
- no scene-item transform mutation;
- GPU zoom/pan rendering;
- OBS pass-through while visually idle.

A Windows CI microbenchmark is published with the build. Hosted-runner timing is diagnostic rather than a cross-machine marketing claim.

## Scene safety

ArZoom v0.2.0 remains a video filter for Display Capture. Camera calculations drive the filter shader and **do not rewrite the original OBS scene-item transforms**. Invalid/missing shader resources fall back to safe pass-through instead of intentionally blacking the source.

## Compatibility

| Item | Current support |
|---|---|
| Operating system | Windows 10/11 x64 |
| OBS source | Display Capture |
| Current build target | OBS Studio 31.1.1 |
| OBS 32.x | Forward validation in progress |
| Zoom range | `1.10×` to `4.00×` |
| Follow modes | Smart Camera, Centered, Fixed |
| Multi-monitor | Supported, including negative desktop coordinates |
| Mixed DPI | Implemented; broader physical-device validation in progress |
| Normal OBS installer | Supported |
| OBS Portable/custom root installer | Supported |
| macOS / Linux | Not available in the current public trial |
| Window Capture / Game Capture | Not part of the current filter MVP |

## Deterministic engineering gates

Before Windows packaging, CI runs platform-neutral motion tests covering:

- 200,000 randomized viewport/edge invariants;
- hand jitter and explanatory gestures;
- focus-preserving activation at center, edges, and corners for 2×/3×/4×;
- activation focus latch and handoff;
- ballistic long relocation and full settle;
- edge-safe zoom-out/return;
- 30/60/120/144 fps convergence;
- multi-monitor normalized-coordinate traces;
- motion microbenchmarks.

Run locally on Windows PowerShell:

```powershell
./scripts/run-phase0-validation.ps1
```

## Build from source

Requirements:

- Windows 10/11 x64
- Git
- CMake 3.28+
- Visual Studio 2022 or 2026 with Desktop development with C++
- Inno Setup 6 for EXE installer packaging

Run:

```text
build-local-windows.bat
```

The first build prepares the OBS plugin-template dependencies. Later builds reuse `.build/`. To package an already successful build without recompiling OBS dependencies, run:

```text
package-existing-build.bat
```

## Project documentation

- [Smart Camera Phase 1 specification](docs/SMART_CAMERA_PHASE1_SPEC.md)
- [Smart Camera tuning gates](docs/SMART_CAMERA_PHASE1_TUNING.md)
- [Phase 0 baseline](docs/PHASE0_BASELINE.md)
- [Getting started](https://masarray.github.io/arzoom-follow-obs/guide.html)
- [Troubleshooting](https://masarray.github.io/arzoom-follow-obs/troubleshooting.html)
- [Latest release notes](https://github.com/masarray/arzoom-follow-obs/releases/latest)
- [Support policy](SUPPORT.md)
- [Contributing](CONTRIBUTING.md)
- [Security policy](SECURITY.md)
- [Changelog](CHANGELOG.md)

## Privacy

ArZoom does not send analytics, cursor data, captured video, or presentation content anywhere.

## License

ArZoom is open-source software licensed under [GPL-2.0-or-later](LICENSE).

OBS Studio is a trademark of its respective owners. ArZoom is an independent community project and is not affiliated with or endorsed by the OBS Project.
