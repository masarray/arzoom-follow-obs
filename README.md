# ArZoom for OBS

<p align="center">
  <img alt="ArZoom logo" src="docs/assets/favicon-192.png" width="156">
</p>

<p align="center">
  <strong>Scene-wide Smart Camera + Presenter Controls + GPU Spotlight for OBS.</strong><br>
  Zoom where you are explaining, stay steady while you teach, and guide attention without rewriting the OBS scene.
</p>

<p align="center">
  <a href="https://github.com/masarray/arzoom-follow-obs/releases/latest/download/ArZoom-OBS-Setup-windows-x64.exe"><img alt="Download ArZoom installer" src="https://img.shields.io/badge/Download-Windows%20installer-2563eb?style=for-the-badge&logo=windows11&logoColor=white"></a>
  <a href="https://masarray.github.io/arzoom-follow-obs/"><img alt="ArZoom website" src="https://img.shields.io/badge/Open-Setup%20guide-0f766e?style=for-the-badge"></a>
</p>

<p align="center">
  <img alt="Version 0.7.0" src="https://img.shields.io/badge/version-0.7.0-0f766e">
  <img alt="Windows 10 and 11" src="https://img.shields.io/badge/Windows-10%20%7C%2011-0078d4?logo=windows11&logoColor=white">
  <img alt="OBS Studio plugin" src="https://img.shields.io/badge/OBS%20Studio-native%20plugin-302e31?logo=obsstudio&logoColor=white">
  <a href="LICENSE"><img alt="GPL 2.0 or later" src="https://img.shields.io/badge/license-GPL--2.0--or--later-blue"></a>
</p>

ArZoom is a native OBS presentation-camera plugin for tutorials, engineering training, software demonstrations, online classes, product walkthroughs, and live streaming.

**v0.7.0 is the current Windows stable release.** It keeps the accepted v0.6.0 P4.1 generalized mapping + Kinematic Smart Viewport baseline and adds the accepted P5 Spotlight presentation layer, cinematic Zoom-linked focus animation, zero-refresh Spotlight controls, and resize-only Spotlight behavior for Zoom +/-.

> **Current status:** Phase 0 through P5 Spotlight are shipped on Windows. The stable camera/mapping contract remains [`docs/P4_1_STABLE_BASELINE.md`](docs/P4_1_STABLE_BASELINE.md); the accepted Spotlight contract is [`docs/P5_STABLE_BASELINE.md`](docs/P5_STABLE_BASELINE.md).

<p align="center">
  <img alt="ArZoom Scene Camera running in OBS Studio" src="docs/assets/product-screenshot.png" width="920">
</p>

## Highlights in v0.7.0

### Cinematic Spotlight with Zoom

When Spotlight controls are enabled, Toggle Zoom can automatically guide attention with a restrained cinematic iris:

```text
full scene
   ↓ Toggle Zoom ON
full-frame aperture → smooth Circle focus
background dim follows gently
   ↓
focused presentation
   ↓ Toggle Zoom OFF
Circle opens → full bright context
```

The animation is time-based, minimum-jerk, reversible mid-transition, and does not create a second camera authority.

Accepted default Spotlight presentation settings:

- mode: **Follow cursor**;
- shape: **Circle**;
- area size: **170%**;
- background dim: **35%**;
- edge softness: **40 px**;
- Presentation Cursor: **ArZoom Classic Hand**;
- Cinematic Spotlight with Zoom: **On** after Spotlight controls are enabled;
- cinematic speed: **Balanced**.

The master **Enable Spotlight controls** setting remains Off by default, so adding ArZoom never places a Spotlight on-air without presenter opt-in.

### Zoom +/- is resize-only

Toggle Zoom ON/OFF owns the cinematic open/close choreography. While Zoom is already active, **Zoom In / Zoom Out only resize the camera, Presentation Cursor, and Spotlight smoothly**; they do not replay the cinematic focus animation.

### D3D11-safe shared shader ABI

Direct OBS testing found and fixed a real black-frame failure in the P5 development path. Every shared Draw parameter now receives deterministic values before processed rendering. The accepted build has zero click/Zoom/cursor black-preview regression and no Spotlight Properties rebuild flicker in direct OBS testing.

## Scene Camera architecture

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
SceneViewportPlanner (WHERE)
       ↓
SceneKinematicMotion (HOW)
       ↓
Presenter Controls / click / cursor / Spotlight
       ↓
final scene output
```

ArZoom intentionally does **not** implement scene-wide zoom by persistently rewriting scene-item position, scale, rotation, bounds, or crop.

Architecture invariants:

- no persistent scene-item transform mutation;
- no custom scene-wide Camera input source;
- no duplicate scene-render graph;
- no CPU frame readback;
- no helper source for Spotlight;
- no second semantic Smart Camera/planner;
- bounded O(1) presentation state;
- OBS pass-through remains available when presentation effects are inactive.

## Install

### Recommended: Windows installer

**[Download ArZoom-OBS-Setup-windows-x64.exe](https://github.com/masarray/arzoom-follow-obs/releases/latest/download/ArZoom-OBS-Setup-windows-x64.exe)**

Close OBS completely before installation. The installer supports Standard OBS Studio and OBS Portable/custom roots containing `bin\64bit\obs64.exe`.

A manual ZIP and SHA-256 checksums are also published with each stable release.

## Recommended workflow

1. Put one visible top-level **Display Capture** in the presentation scene.
2. In OBS choose **Tools → ArZoom — Toggle Scene Camera**.
3. Choose **Tools → ArZoom — Configure Scene Camera**.
4. Disable old per-source ArZoom filters while Scene Camera is active to avoid double zoom.
5. If using ArZoom Presentation Cursor, disable the native Display Capture cursor to avoid a double cursor.
6. Assign presenter controls in **OBS Settings → Hotkeys**.
7. For Spotlight, enable **Spotlight controls** and leave **Cinematic Spotlight with Zoom** enabled for the recommended v0.7.0 presentation behavior.

The existing per-source **ArZoom Filter** remains available for the lightest Display Capture-only workflow.

## Mapping safety

Scene-wide pointer-driven Smart Follow, click anchoring, Presentation Cursor, and Cursor Spotlight use the same read-only Display Capture → scene mapping.

Supported stable scope:

- exactly one visible top-level Display Capture as presentation owner;
- deterministically resolved captured monitor;
- positive axis-aligned fullscreen, scaled, or inset placement;
- crop-aware mapping when proven;
- shared mapping for Smart Follow, click, cursor, and Spotlight focus.

ArZoom fails safe instead of guessing when ownership or geometry is ambiguous. Deterministic multi-capture ownership remains future P4.2 work.

## Presenter Controls

Profile-persistent OBS hotkeys include:

- Toggle Zoom;
- Hold Zoom;
- Freeze Camera;
- Toggle Smart Follow;
- Zoom In / Zoom Out;
- Reset / Full Frame;
- Overview Peek;
- Toggle Spotlight;
- Hold Spotlight.

## Presentation Cursor and click feedback

Seven built-in ArZoom cursor presets are available, including **Classic Hand** and **Sticker Hand**. Cursor size follows the exact live camera zoom and keeps its hotspot aligned through camera motion.

Click feedback is GPU-native, content-anchored, bounded, and cannot wake or retarget camera intent.

## Performance principles

ArZoom does not require AI, OCR, image recognition, cloud processing, or frame analysis. The hot path is built around deterministic math, one shared presentation pass, bounded state, no growing histories, no per-frame file/settings writes, and no frame readback.

## Compatibility

| Item | v0.7.0 support |
|---|---|
| Operating system | Windows 10/11 x64 |
| Build baseline | OBS Studio 31.1.1 |
| Direct accepted field environment | OBS 32.1.2 / D3D11 |
| Scene Camera | Supported |
| Per-source Display Capture filter | Supported |
| Scene-wide Smart Follow | One proven top-level Display Capture; deterministic axis-aligned fullscreen/scale/inset/crop mapping |
| Spotlight | Smart Focus / Follow cursor / Click to lock |
| Cinematic Spotlight with Zoom | Supported |
| Zoom range | 1.10×–4.00× |
| Multi-monitor | Supported, including negative desktop coordinates |
| Standard OBS installer | Supported |
| OBS Portable/custom root | Supported |
| Multiple Display Capture ownership | Future P4.2; never guessed |
| macOS / Linux | Not included in current release |

## Engineering gates

The Windows pipeline runs deterministic P0–P5 tests before packaging. The v0.7.0 accepted candidate passed **19/19 deterministic tests**, Windows C++/shader compilation, installer packaging, and direct OBS acceptance.

Run locally on Windows PowerShell:

```powershell
./scripts/run-phase0-validation.ps1
```

## Build from source

Requirements: Windows 10/11 x64, Git, CMake 3.28+, Visual Studio 2022+ with Desktop development with C++, and Inno Setup 6 for EXE packaging.

```text
build-local-windows.bat
```

## Contributor / AI development direction

Read in this order:

1. **[Current Project Direction](docs/PROJECT_DIRECTION.md)**
2. **[P4.1 Stable Baseline](docs/P4_1_STABLE_BASELINE.md)**
3. **[P5 Stable Baseline](docs/P5_STABLE_BASELINE.md)**
4. historical phase/design documents only for context.

Do not revive the superseded custom scene-camera source/off-screen-render experiment, persistent scene-item mutation, or a competing semantic camera planner. Do not weaken accepted motion, shader-ABI, or presentation-safety gates merely to make new work pass.

## Privacy

ArZoom does not send analytics, cursor data, captured video, click data, or presentation content anywhere.

## License

ArZoom is open-source software licensed under [GPL-2.0-or-later](LICENSE).

OBS Studio is a trademark of its respective owners. ArZoom is an independent community project and is not affiliated with or endorsed by the OBS Project.
