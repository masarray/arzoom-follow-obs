# ArZoom Runtime Architecture

**Current baseline:** v0.5.x scene-level filter architecture.

For current project direction and superseded approaches, read [`PROJECT_DIRECTION.md`](PROJECT_DIRECTION.md) first.

## Runtime layers

ArZoom separates input, deterministic camera logic, OBS integration, and GPU rendering:

```text
Platform input / OBS hotkeys
        ↓
small input snapshots + presenter commands
        ↓
Smart Camera state machine and math
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
- a second Smart Camera engine;
- CPU frame readback;
- per-frame file I/O or settings writes.

The user's scene composition remains the source of truth. ArZoom changes the filtered output, not persistent scene-item state.

## Camera state flow

The accepted Smart Camera behavior is richer than the original binary zoom state:

```text
REST
  ↓ activate
ACTIVATING
  ↓
SMOOTH_IDLE
  ↕
OBSERVE
  ↓ real relocation
FOLLOW / CATCH_UP
  ↓
COAST
  ↓
SMOOTH_IDLE

RETURNING → REST
```

Presenter controls such as Freeze Camera and Overview Peek temporarily alter camera intent/state without creating a separate motion engine.

## Zoom trajectory

Zoom transitions use deterministic minimum-jerk motion and preserve the chosen focus point. Completed idle states settle exactly rather than continuously micro-correcting.

The visible viewport must always remain within valid source/scene bounds.

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

ArZoom must distinguish these spaces instead of assuming they are identical:

1. Windows virtual-desktop pixels;
2. selected monitor-local pixels;
3. Display Capture source coordinates;
4. scene-item local/draw coordinates;
5. OBS scene canvas coordinates;
6. normalized ArZoom camera coordinates;
7. final filtered output coordinates.

A fullscreen top-level Display Capture makes several of these transforms effectively identity mappings, which is why v0.5.x can prove that case safely.

Complex layouts require explicit read-only transform composition.

## Scene-wide mapping contract

Pointer-driven Scene Camera features are enabled only when ArZoom can prove a deterministic desktop-to-scene mapping.

Current v0.5.x proven case:

- exactly one visible top-level Display Capture;
- it fills the scene canvas;
- the captured monitor can be resolved deterministically.

If scale, crop, rotation, nesting, or multiple Display Captures make ownership/mapping ambiguous, ArZoom must fail safe instead of guessing.

Future mapping work should read scene geometry using APIs such as `obs_sceneitem_get_box_transform()` / `obs_sceneitem_get_draw_transform()` and compose/invert those transforms without writing scene-item state.

## Presentation effects

GPU click feedback and Presentation Cursor are presentation-output effects. Their state is bounded and they must not wake, retarget, or accelerate Smart Camera motion.

When presentation effects are inactive, OBS pass-through should remain available.

## Lifecycle contract

Scene/source changes, filter removal, OBS shutdown, monitor topology changes, and invalid pointer mapping are first-class state transitions.

The safe fallback is always:

- stop pointer-driven retargeting when mapping is not proven;
- preserve presenter controls/fixed framing where safe;
- leave the user's scene-item transforms untouched.
