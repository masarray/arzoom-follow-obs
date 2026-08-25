# ArZoom Runtime Architecture

**Current baseline:** v0.6.0 scene-level filter + P4.1 generalized read-only mapping + Kinematic Smart Viewport.

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
SceneKinematicMotion (HOW)
        ↓
OBS source/filter integration
        ↓
GPU presentation rendering
```

The camera model does not depend on a private duplicate render graph and does not require persistent changes to the user's scene-item transforms.

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

## Architecture invariants

Scene Camera must not be reimplemented using:

- `obs_sceneitem_set_pos` / `obs_sceneitem_set_scale` / rotation / bounds / crop writes to the user's composition;
- a custom scene-wide `ArZoom Camera` input source;
- a duplicate off-screen scene render solely for zoom/follow;
- a second semantic Smart Camera/planner;
- CPU frame readback;
- per-frame file I/O or settings writes.

The user's scene composition remains the source of truth. ArZoom changes the filtered output, not persistent scene-item state.

## Scene Camera motion authority

P4.1 deliberately separates semantic framing from physical motion inside one camera authority:

- `SceneViewportPlanner` owns pointer/context semantics and decides **WHERE** to frame.
- `SceneKinematicMotion` owns position/velocity/acceleration continuity and decides **HOW** to reach that frame.

The kinematic synthesizer is not a second semantic engine. It has no OBS scene ownership or pointer-intent policy.

The accepted motion contract includes bounded jerk, no artificial stop/restart at tracking→settle, bounded far-distance cruise for moving targets, automatic jerk-aware precision braking for immutable targets, and exact drift-free HOLD.

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

## Presentation effects

GPU click feedback and Presentation Cursor are presentation-output effects. Their state is bounded and they must not wake, retarget, or accelerate the semantic camera planner.

When presentation effects are inactive, OBS pass-through should remain available.

## Lifecycle contract

Scene/source changes, filter removal, OBS shutdown, monitor topology changes, and invalid pointer mapping are first-class state transitions.

The safe fallback is always:

- stop pointer-driven retargeting when mapping is not proven;
- preserve presenter controls/fixed framing where safe;
- leave the user's scene-item transforms untouched.

## Regression contract

The v0.6.0 P4.1 motion and mapping contracts are frozen in [`P4_1_STABLE_BASELINE.md`](P4_1_STABLE_BASELINE.md). User-visible camera-motion changes must preserve both pointer acquisition and smooth kinematic quality and require direct OBS trial before replacing that baseline.
