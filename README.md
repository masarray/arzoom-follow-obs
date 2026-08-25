# ArZoom for OBS

<p align="center">
  <img alt="ArZoom logo" src="docs/assets/favicon-192.png" width="156">
</p>

<p align="center">
  <strong>Scene-wide Smart Camera + Presenter Controls + GPU presentation feedback for OBS.</strong><br>
  Zoom where you are explaining, stay steady while you teach, and keep the OBS scene intact.
</p>

<p align="center">
  <a href="https://github.com/masarray/arzoom-follow-obs/releases/latest/download/ArZoom-OBS-Setup-windows-x64.exe"><img alt="Download ArZoom installer" src="https://img.shields.io/badge/Download-Windows%20installer-2563eb?style=for-the-badge&logo=windows11&logoColor=white"></a>
  <a href="https://masarray.github.io/arzoom-follow-obs/"><img alt="ArZoom website" src="https://img.shields.io/badge/Open-Setup%20guide-0f766e?style=for-the-badge"></a>
</p>

<p align="center">
  <img alt="Version 0.5.1" src="https://img.shields.io/badge/version-0.5.1-0f766e">
  <img alt="Windows 10 and 11" src="https://img.shields.io/badge/Windows-10%20%7C%2011-0078d4?logo=windows11&logoColor=white">
  <img alt="OBS Studio plugin" src="https://img.shields.io/badge/OBS%20Studio-native%20plugin-302e31?logo=obsstudio&logoColor=white">
  <a href="LICENSE"><img alt="GPL 2.0 or later" src="https://img.shields.io/badge/license-GPL--2.0--or--later-blue"></a>
</p>

ArZoom is a native OBS presentation-camera plugin for tutorials, engineering training, software demonstrations, online classes, product walkthroughs, and live streaming.

**v0.5.1 introduces ArZoom's complete visual identity across the website, Windows installer, plugin binary, favicons, screenshots, and social previews.** ArZoom Scene Camera remains the managed filter attached directly to the current OBS scene.

> **Current status:** v0.5.1 public Windows release. The core Scene Camera workflow, Smart Zone camera, Presenter Controls, Overview Peek, GPU click visualization, Presentation Cursor, installer, and deterministic regression gates are implemented. Wider OBS/GPU/mixed-DPI validation continues on the road to v1.0.

<p align="center">
  <img alt="ArZoom Scene Camera running in OBS Studio" src="docs/assets/product-screenshot.png" width="920">
</p>

## Why Scene Camera

The primary workflow is scene-wide without rewriting the user's composition:

```text
OBS Scene
├─ Display Capture
├─ Webcam
├─ Browser
├─ Logo
└─ Nested Scene
       ↓ OBS native composition
scene obs_source_t
       ↓ normal OBS effect-filter chain
ArZoom Camera
       ↓
Smart Zone / Presenter Controls / click / cursor
       ↓
final scene output
```

ArZoom intentionally does **not** implement scene-wide zoom by calling `obs_sceneitem_set_pos`, `obs_sceneitem_set_scale`, `obs_sceneitem_set_rot`, bounds setters, or crop setters on the user's composition.

That means:

- no persistent scene-item transform mutation;
- no transform recovery map needed;
- no hidden helper scene item;
- no CPU frame readback;
- no duplicate scene-render graph;
- no second Smart Camera engine.

## Install

### Recommended: Windows installer

Download:

**[ArZoom-OBS-Setup-windows-x64.exe](https://github.com/masarray/arzoom-follow-obs/releases/latest/download/ArZoom-OBS-Setup-windows-x64.exe)**

Close OBS completely before installation.

The installer supports:

- **Standard OBS Studio** — auto-detected when possible;
- **OBS Portable / custom OBS folder** — browse to the OBS root containing `bin\64bit\obs64.exe`.

The v0.5.1 installer includes the canonical ArZoom icon and version metadata, and validates the destination before copying files.

### Manual ZIP

A manual ZIP remains available in Releases. Merge its `obs-plugins` and `data` folders into the OBS root folder.

## Recommended v0.5.1 workflow

1. Put one fullscreen **Display Capture** in the scene you want to present.
2. In OBS choose **Tools → ArZoom — Toggle Scene Camera**.
3. Choose **Tools → ArZoom — Configure Scene Camera** to open the managed scene filter.
4. Disable old per-source ArZoom filters while Scene Camera is active to avoid double zoom.
5. If using an ArZoom Presentation Cursor, disable the native Display Capture cursor to avoid a double cursor.
6. Assign presenter controls in **OBS Settings → Hotkeys**.

The existing per-source **ArZoom Filter** remains available for the lightest Display Capture-only workflow.

## Scene-wide mapping safety

Pointer-driven Smart Follow, click anchoring, and Presentation Cursor are enabled only when ArZoom can prove a deterministic Display Capture → scene mapping.

The initial supported scene-wide mapping is intentionally conservative:

- exactly one visible top-level Display Capture;
- its OBS scene-item box transform fills the full scene canvas;
- its monitor resolves deterministically.

If the scene contains multiple Display Captures or the capture is inset/scaled/rotated/cropped in a way that makes cursor mapping ambiguous, ArZoom does **not** guess. Presenter zoom/freeze/overview and safe fixed framing remain available while pointer-driven mapping is unavailable.

## Smart Zone Gimbal Camera

ArZoom follows presentation areas rather than chasing every mouse movement.

- **Smart Zone:** local pointer movement can occur without moving the camera.
- **Follow / Catch-Up:** real relocation starts a smooth shot toward the new explanation area.
- **Coast:** live pointer influence fades naturally before the camera settles.
- **SmoothIdle:** pointing, circling, and hand jitter inside the current area remain steady.
- **Minimum-jerk zoom:** focus-preserving zoom-in and wobble-free zoom-out use smooth quintic trajectories.
- **Exact settle:** completed idle states do not breathe or micro-correct.
- **Camera characters:** Cinematic, Balanced, Responsive.

Conceptually:

```text
ACTIVATING
    ↓
SMOOTH_IDLE ↔ OBSERVE → FOLLOW / CATCH_UP → COAST → SMOOTH_IDLE
    ↓
RETURNING → REST
```

## Presenter Controls

ArZoom includes profile-persistent OBS hotkeys for:

- Toggle Zoom;
- Hold Zoom;
- Freeze Camera;
- Toggle Smart Follow;
- Zoom In / Zoom Out;
- Reset / Full Frame;
- Overview Peek — hold for a temporary exact 1× overview, release to restore the exact saved shot.

## GPU Click Visualization

Click feedback is composed in the same presentation GPU pass and cannot wake, retarget, or accelerate Smart Zone camera motion.

- left: Azure + Aqua dual analytic rings;
- right: Violet + Orchid;
- middle: compact Amber + Gold;
- content-anchored while camera zoom/pan moves;
- fixed four-slot allocation-free event state;
- no PNG generation, particles, CPU rasterization, or frame readback.

## Presentation Cursor

Seven built-in ArZoom-native cursor presets are available:

- Prism;
- Outline;
- Azure;
- Orchid;
- Parakeet;
- Classic Hand;
- Sticker Hand.

Built-ins use short tactile click micro-interactions with exact return to the idle pose. Arrow-tip/fingertip hotspots remain aligned through camera zoom and pan. Advanced users may use custom GIF/WebP/PNG assets.

v0.5.0 also hardens the shared GPU pass so a fresh installation cannot enter the click render path with an unbound cursor sampler before any cursor style has ever been selected.

## Performance principles

ArZoom uses deterministic math and GPU-native presentation effects. It does not require AI, OCR, image recognition, or frame analysis.

The hot path is designed around:

- bounded fixed-size camera/control state;
- one cursor sample per active video tick;
- fixed click-event slots;
- no growing history or particle containers;
- no frame readback;
- no per-frame file I/O;
- no per-frame settings writes;
- no scene-item transform mutation;
- OBS pass-through when presentation effects are inactive.

## Compatibility

| Item | v0.5.1 support |
|---|---|
| Operating system | Windows 10/11 x64 |
| Build baseline | OBS Studio 31.1.1 |
| OBS 32.x | Forward validation continues |
| Scene Camera | Supported |
| Per-source Display Capture filter | Supported |
| Scene-wide Smart Follow | One proven fullscreen Display Capture mapping |
| Zoom range | 1.10×–4.00× |
| Multi-monitor | Supported, including negative desktop coordinates |
| Mixed DPI | Implemented; broader device validation continues |
| Standard OBS installer | Supported |
| OBS Portable/custom root | Supported |
| macOS / Linux | Not included in current release |

## Deterministic engineering gates

Before Windows packaging, CI runs platform-neutral regression tests covering:

- randomized viewport/edge invariants;
- hand jitter and explanation gestures;
- focus-preserving straight zoom-in and stable zoom-out;
- Follow → Coast → SmoothIdle;
- local explanation orbit rejection;
- soft wake-up and continuous retargeting;
- rapid zone switching;
- 2×/3×/4× edge/corner stress;
- 30/60/120/144 fps behavior;
- fixed click capacity and deterministic expiry;
- content anchoring and camera isolation;
- Presenter Controls and Overview Peek restoration;
- Presentation Cursor playback/hotspot gates;
- Scene Camera identity and fullscreen mapping rules;
- hard rejection of P4 `obs_sceneitem_set_*` mutation;
- first-click cursor-sampler render safety.

Run locally on Windows PowerShell:

```powershell
./scripts/run-phase0-validation.ps1
```

## Build from source

Requirements:

- Windows 10/11 x64;
- Git;
- CMake 3.28+;
- Visual Studio 2022 or newer with Desktop development with C++;
- Inno Setup 6 for EXE packaging.

Run:

```text
build-local-windows.bat
```

## Contributor / AI development direction

Before proposing or implementing architectural work, read **[Current Project Direction](docs/PROJECT_DIRECTION.md)**. It is the source of truth for current priorities and for approaches that were explored but explicitly rejected.

In particular, Phase 4 is **complete**. Scene Camera is a managed scene-level `arzoom_filter`; do not revive the superseded custom `ArZoom Camera` input-source/off-screen-render experiment or Zoominator-style persistent scene-item transform mutation.

## Project documentation

- **[Current Project Direction — start here](docs/PROJECT_DIRECTION.md)**
- [Runtime architecture](docs/ARCHITECTURE.md)
- [Phase 4 Zoominator / architecture decision](docs/PHASE4_ZOOMINATOR_AUDIT.md)
- [Smart Camera architecture contract](docs/SMART_CAMERA_ARCHITECTURE.md)
- [Presentation Cursor](docs/PRESENTATION_CURSOR_PRESETS.md)
- [Presenter Controls](docs/PRESENTER_CONTROLS_PHASE3.md)
- [Getting started](https://masarray.github.io/arzoom-follow-obs/guide.html)
- [Troubleshooting](https://masarray.github.io/arzoom-follow-obs/troubleshooting.html)
- [Latest release](https://github.com/masarray/arzoom-follow-obs/releases/latest)

## Privacy

ArZoom does not send analytics, cursor data, captured video, click data, or presentation content anywhere.

## License

ArZoom is open-source software licensed under [GPL-2.0-or-later](LICENSE).

OBS Studio is a trademark of its respective owners. ArZoom is an independent community project and is not affiliated with or endorsed by the OBS Project.
