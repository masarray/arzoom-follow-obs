# ArZoom for OBS

<p align="center">
  <strong>Smart Zone presentation camera for OBS screen capture.</strong><br>
  Zoom where you are presenting, stay steady while you explain, and glide smoothly when you move to another area.
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

ArZoom is a native **OBS presentation-camera plugin** for tutorials, engineering training, software demonstrations, online classes, product walkthroughs, and live streaming. It applies zoom/pan to a Display Capture source through a GPU filter while the camera interprets cursor movement as **presentation intent**, not as a command to chase every mouse movement.

> **Current status:** v0.2.0 public trial for Windows. Smart Zone camera motion, straight zoom transitions, edge safety, global hotkey persistence, standard/portable-aware installation, deterministic motion tests, and fail-safe rendering are implemented. Broader hardware and OBS-version validation continues before v1.0.

## What changed in v0.2.0

The v0.1.x runtime behaved like an edge follower. v0.2.0 replaces that behavior with a **Smart Zone Gimbal Camera**.

- **Focus-preserving zoom-in:** edge/corner activation goes directly toward the intended subject instead of zooming into unrelated center content first.
- **Straight screen-space zoom:** zoom-in and zoom-out interpolate one affine screen transform with quintic minimum-jerk easing, so the visible image does not bow sideways during the transition.
- **Smart Zone:** ArZoom follows meaningful presentation-area changes, not every local pointer movement.
- **SmoothIdle:** circling a button, pointing around a diagram, or moving the mouse while explaining keeps the viewport steady.
- **Coast handoff:** after a real relocation, camera influence fades gradually into SmoothIdle instead of visibly snapping from moving to steady.
- **Soft wake-up:** leaving the outer Smart Zone starts a new follow shot with a gentle first movement step.
- **Continuous retargeting:** changing destination while the camera is already moving bends the existing path instead of restarting animation.
- **Filtered edge urgency:** when the pointer is genuinely at risk of being lost, the same gimbal response becomes smoothly more responsive without switching to different physics.
- **Exact settle:** completed idle and zoom-out states do not breathe or micro-correct.

Conceptually:

```text
ACTIVATING
    ↓
SMOOTH_IDLE ↔ OBSERVE → FOLLOW / CATCH_UP → COAST → SMOOTH_IDLE
    ↓
RETURNING → REST
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
- browse to the OBS root folder you actually launch;
- a valid root contains `bin\64bit\obs64.exe`.

Example:

```text
D:\PortableApps\OBS\
├─ bin\64bit\obs64.exe
├─ obs-plugins\
└─ data\
```

Select `D:\PortableApps\OBS\`, not `bin\64bit` and not `obs-plugins`.

The installer remembers the last valid custom OBS root for future upgrades.

### Manual ZIP

A manual ZIP remains available in Releases. Merge its `obs-plugins` and `data` folders into the OBS root folder. This is useful for direct extraction or multiple portable OBS copies.

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
| Camera follow | `Smart Camera` | Presentation-area tracking |
| Camera character | `Balanced` | Recommended stability and travel speed |
| Stable comfort zone | `28%` | Lets normal explanatory pointer movement happen without camera shake |
| Target monitor | `Auto` | Maps the Display Capture monitor automatically |

Camera character options:

- **Cinematic** — largest steady zone and softest/slowest movement.
- **Balanced** — recommended default.
- **Responsive** — shorter intent delay and faster gimbal response while preserving no-snap behavior.

## Why ArZoom stays lightweight

Smart Camera intelligence is deterministic vector math; it does not require AI, OCR, computer vision, or frame analysis.

The hot path is designed around:

- one cursor sample per active video tick;
- fixed-size camera state;
- no growing history containers;
- no frame readback;
- no image analysis;
- no per-frame file I/O;
- no per-frame OBS settings writes;
- no scene-item transform mutation;
- GPU zoom/pan rendering;
- cheap SmoothIdle camera logic while the presenter remains in one area;
- OBS pass-through while zoom is inactive.

Hosted-runner microbenchmark numbers are engineering diagnostics, not cross-machine marketing claims.

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

Before Windows packaging, CI runs platform-neutral tests covering:

- 200,000 randomized viewport/edge invariants;
- hand jitter and explanation gestures;
- straight focus-preserving zoom-in;
- straight wobble-free zoom-out;
- Follow → Coast → SmoothIdle while the mouse may continue moving;
- local explanation orbit remaining stationary after relocation;
- soft SmoothIdle wake-up for the next presentation area;
- continuous retargeting with no one-frame snap;
- rapid zone switching;
- 2×/3×/4× Smart Zone and corner-return stress;
- 30/60/120/144 fps behavior;
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

- [Smart Zone Phase 1 specification](docs/SMART_CAMERA_PHASE1_SPEC.md)
- [Phase 1 closeout/tuning gates](docs/SMART_CAMERA_PHASE1_TUNING.md)
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
