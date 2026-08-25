# ArZoom Runtime Architecture

**Current baseline:** v0.6.0 scene-level filter + P4.1 generalized read-only mapping + Kinematic Smart Viewport.

**Planned extension:** P5 Smart Focus Spotlight + Beginner-First GUI, specified in [`P5_SMART_FOCUS_SPOTLIGHT_UX.md`](P5_SMART_FOCUS_SPOTLIGHT_UX.md). P5 is not shipped in v0.6.0.

For current project direction and the accepted regression lock, read [`PROJECT_DIRECTION.md`](PROJECT_DIRECTION.md) and [`P4_1_STABLE_BASELINE.md`](P4_1_STABLE_BASELINE.md) first.

## Runtime layers

ArZoom separates input, deterministic camera logic, OBS integration, and GPU rendering:

```text
Platform input / OBS hotkeys
        ↓
small input snapshots + presenter commands
        ↓
read-only scene mapping
        ↓
SceneViewportPlanner (WHERE)
        ↓
        ├──────── semantic focus (read-only) ────────┐
        ↓                                            ↓
SceneKinematicMotion (HOW)                  Presentation-effect state
        ↓                                   click / cursor / Spotlight
        └──────────────────────┬─────────────────────┘
                               ↓
                    OBS filter integration
                               ↓
                  GPU presentation rendering
```

The camera model does not depend on a private duplicate render graph and does not require persistent changes to the user's scene-item transforms.

Presentation effects are one-way consumers of mapped/presenter/camera state. They cannot become camera authorities.

## Two supported placement modes

### Per-source ArZoom Filter

For the lightest Display Capture-only workflow, `arzoom_filter` can remain attached directly to a supported source.

```text
Display Capture
      ↓
ArZoom Filter
      ↓
scene composition
```

### Scene-level ArZoom Camera

For scene-wide presentation, ArZoom attaches a managed instance of the same `arzoom_filter` to the current OBS scene source:

```text
OBS Scene
├─ Display Capture
├─ Webcam
├─ Browser
├─ Logo
└─ Nested Scene
       ↓ OBS native composition
scene obs_source_t
       ↓ OBS source-filter pipeline
ArZoom Camera
       ↓
final scene output
```

This is the accepted Phase 4 architecture.

P5 Spotlight must extend the filter's existing presentation pass rather than introduce a new source-placement model.

## Architecture invariants

Scene Camera and presentation effects must not be reimplemented using:

- `obs_sceneitem_set_pos` / `obs_sceneitem_set_scale` / rotation / bounds / crop writes to the user's composition;
- a custom scene-wide `ArZoom Camera` input source;
- a duplicate off-screen scene render solely for zoom/follow/Spotlight;
- a second semantic Smart Camera/planner;
- a Spotlight-specific semantic planner;
- CPU frame readback;
- per-frame file I/O or settings writes;
- generated PNG/browser overlays as the production Spotlight;
- a helper scene item for dimming/masking;
- default multi-pass blur for Spotlight.

The user's scene composition remains the source of truth. ArZoom changes the filtered output, not persistent scene-item state.

## Scene Camera motion authority

P4.1 deliberately separates semantic framing from physical motion inside one camera authority:

- `SceneViewportPlanner` owns pointer/context semantics and decides **WHERE** to frame.
- `SceneKinematicMotion` owns position/velocity/acceleration continuity and decides **HOW** to reach that frame.

The kinematic synthesizer is not a second semantic engine. It has no OBS scene ownership or pointer-intent policy.

The accepted motion contract includes bounded jerk, no artificial stop/restart at tracking→settle, bounded far-distance cruise for moving targets, automatic jerk-aware precision braking for immutable targets, and exact drift-free HOLD.

P5 Smart Spotlight may consume a read-only semantic-focus seam from the planner. It must not write target/pressure/latch state back into either camera layer.

## Zoom trajectory

Explicit Zoom In/Out trajectories remain deterministic and focus-aware. The visible viewport must remain inside valid source/scene bounds.

For normalized zoom `z` in the simple source-aligned case:

```text
half viewport = 0.5 / z
center ∈ [half viewport, 1 - half viewport]
```

Therefore:

```text
center - half viewport >= 0
center + half viewport <= 1
```

Equivalent safety rules apply after coordinate transforms in scene-wide mapping.

## Coordinate systems

ArZoom distinguishes these spaces instead of assuming they are identical:

1. Windows virtual-desktop pixels;
2. selected monitor-local pixels;
3. Display Capture source coordinates;
4. scene-item local/draw coordinates;
5. OBS scene canvas coordinates;
6. normalized ArZoom camera coordinates;
7. final filtered output coordinates.

P5 adds no new coordinate authority. Spotlight consumes the same proven spaces/transforms.

### Planned Spotlight coordinate rule

Spotlight center and size intentionally have different anchoring semantics:

- **center** may be content/presentation anchored;
- **size** remains output-space stable.

Examples:

```text
Click Spotlight:
content anchor
   ↓ current camera transform
output-space Spotlight center
   + output-space stable size

Cursor Spotlight:
mapped cursor
   ↓ current camera transform
output-space Spotlight center
   + output-space stable size

Smart Spotlight:
planner semantic focus
   ↓ current camera transform
output-space Spotlight center
   + output-space stable size
```

This keeps a locked Click focus attached to the same content while preventing the apparent Spotlight size from growing/shrinking unpredictably with camera zoom.

## P4.1 scene-wide mapping contract

Pointer-driven Scene Camera features are enabled only when ArZoom can prove a deterministic desktop-to-scene mapping.

v0.6.0 proven scope:

- exactly one visible top-level Display Capture owns the mapping;
- the captured monitor resolves deterministically;
- positive axis-aligned fullscreen, scaled, or inset placement can be mapped;
- crop-aware mapping is supported when the transform can be proven;
- Smart Follow, click anchoring, and Presentation Cursor use one mapped coordinate path;
- Presentation Cursor size follows exact live camera zoom.

ArZoom fails safe for unsupported/ambiguous cases including multiple candidate Display Captures, unproven rotation/skew/flips, unsupported bounds modes, unresolved nested capture ownership, or invalid monitor/source geometry.

Mapping remains read-only. Future mapping work may extend transform composition, but must not write scene-item state to make mapping easier.

Planned Cursor/Click Spotlight inherits the same mapping availability. P4.2 multi-capture selection must extend the shared mapping layer, after which Spotlight inherits it naturally.

## Presentation effects

GPU click feedback and Presentation Cursor are presentation-output effects. Their state is bounded and they must not wake, retarget, or accelerate the semantic camera planner.

Planned P5 Spotlight joins this same effect layer with exactly three user-facing behavior modes:

- **Smart Focus** — consumes read-only semantic camera focus;
- **Cursor** — consumes the mapped pointer;
- **Click** — stores one content-space focus anchor.

All three modes remain camera-isolated.

### Planned Spotlight GPU path

Preferred implementation:

```text
composed scene texture
        ↓ one normal ArZoom source sample
camera transform
        ↓
analytic Spotlight mask
        + click/cursor presentation math
        ↓
final output
```

The mask should use cheap analytic math such as ellipse or rounded-rectangle signed-distance/smoothstep operations.

Default effect characteristics:

- center remains original brightness;
- outside region is multiplicatively dimmed;
- broad feather;
- no blur pass;
- no bloom/particles;
- no extra scene render;
- compact uniforms/state;
- pass-through/inert path when Spotlight is Off.

## Planned Spotlight state

A compact effect-only state is permitted conceptually:

```text
SpotlightState
├─ enabled
├─ mode
├─ target_center
├─ visual_center
├─ locked_content_anchor
├─ opacity/fade state
├─ size
├─ dim strength
├─ feather
└─ shape
```

State must remain O(1), time-based, frame-rate independent, and free of growing history.

Visual-only smoothing is permitted, but it cannot modify camera target generation or follow pressure.

## GUI/property architecture direction

The current v0.6.0 property page is functional but exposes common and technical controls too close together. P5 should use progressive disclosure without requiring a custom dock/window.

Target structure using standard OBS properties/groups:

```text
STATUS

QUICK SETUP
  Enable ArZoom
  Zoom Amount
  Camera Focus
  Camera Motion
  Preview Zoom
  Reset View

SPOTLIGHT
  Enable Spotlight
  Spotlight Mode
  Focus Size
  Background Dim
  Preview Spotlight

CONTROLS
  Hotkey status
  Open OBS Hotkeys

ADVANCED [collapsed]
  Safe Zone
  Target Monitor
  Anchor X/Y
  Reset When Hidden
  Spotlight Shape
  Spotlight Feather
  diagnostics / mapping reason
```

Stored v0.6.0 setting values should remain compatible. Beginner-facing labels may improve without changing the underlying behavior/schema.

Preferred label direction:

- Mouse Follow → **Camera Focus**;
- Smart → **Smart Follow (Recommended)**;
- Centered → **Center on Pointer**;
- Fixed → **Fixed Frame**;
- Movement → **Camera Motion**;
- Smooth → **Smooth (Recommended)**;
- Fast → **Responsive**.

A separate Simple/Expert mode should not be added unless standard grouped progressive disclosure proves insufficient.

## Lifecycle contract

Scene/source changes, filter removal, OBS shutdown, monitor topology changes, and invalid pointer mapping are first-class state transitions.

The safe fallback is always:

- stop pointer-driven retargeting when mapping is not proven;
- preserve presenter controls/fixed framing where safe;
- leave the user's scene-item transforms untouched;
- hold/disable pointer-dependent Spotlight behavior rather than guessing;
- clear transient Spotlight render state safely when the filter is disabled/removed.

## Regression contract

The v0.6.0 P4.1 motion and mapping contracts are frozen in [`P4_1_STABLE_BASELINE.md`](P4_1_STABLE_BASELINE.md). User-visible camera-motion changes must preserve both pointer acquisition and smooth kinematic quality and require direct OBS trial before replacing that baseline.

P5 Spotlight must add tests around effect state, mapping, anchor transforms, frame-rate independence, and mask math while keeping the entire P4.1 suite unchanged.
