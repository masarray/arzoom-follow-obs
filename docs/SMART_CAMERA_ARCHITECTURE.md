# ArZoom Smart Camera Architecture Contract

**Status:** current engineering contract for the accepted Smart Camera and presentation-rendering model.

Read [`PROJECT_DIRECTION.md`](PROJECT_DIRECTION.md) first for current priorities and explicitly superseded directions.

## Product intent

ArZoom should behave like a skilled presentation camera operator, not a robotic mouse follower. Cursor movement is evidence of presenter intent, not a direct camera command.

The accepted camera behavior follows presentation areas, rejects local explanatory hand motion, travels smoothly when relocation is real, coasts naturally, and settles exactly.

## Domain language

| Term | Meaning |
|---|---|
| **Cursor Position** | Platform input sample mapped into the active presentation coordinate space. |
| **Presenter Intent** | Evidence that the presenter wants the audience to look somewhere else. |
| **Smart Zone** | Region where normal hand jitter and explanatory pointer motion can occur without forcing camera movement. |
| **Observe** | State where input is watched without committing to a camera relocation. |
| **Follow / Catch-Up** | Controlled camera travel toward a newly established presentation area. |
| **Coast** | Handoff phase where live pointer influence fades before settling. |
| **SmoothIdle** | Stable explanation state that rejects local jitter/circling and avoids camera breathing. |
| **Camera Center** | Point at the center of the visible viewport in the active camera coordinate space. |
| **Camera Velocity** | Rate of change of Camera Center. |
| **Urgency** | How strongly the camera should prioritize catching up when the presenter meaningfully relocates. |
| **Edge Constraint** | Requirement that the visible viewport never reveals invalid content. |
| **Click Event** | Timestamped, typed pointer click with mapped location. It may drive presentation feedback but must not disturb camera intent. |
| **Presentation Cursor** | ArZoom-rendered cursor/pointer feedback in the presentation pass. |
| **Presentation Event** | Input event that may influence camera intent or presentation effects without coupling camera math to one OS backend. |

## Architecture boundaries

ArZoom separates four responsibilities:

```text
Platform Input / OBS Commands
        ↓
Input Snapshot / Presentation Events
        ↓
Presenter Intent + Smart Camera Math
        ↓
OBS Integration + GPU Presentation Rendering
```

### Platform input

Platform-specific code may read cursor position, buttons, monitor topology, DPI information, or future input devices. It publishes compact snapshots/events rather than embedding camera policy.

### Camera math

Camera math remains platform-independent and deterministic. It owns Smart Zone semantics, camera state transitions, target framing, minimum-jerk zoom, edge constraints, Coast/SmoothIdle behavior, and exact settle.

### GPU presentation rendering

Rendering receives compact camera/effect parameters. It must not depend on CPU frame readback. Click feedback and Presentation Cursor belong in the presentation output path and use bounded state.

### OBS integration

OBS lifecycle, settings, hotkeys, filter ownership, scene selection, mapping validation, and pass-through behavior stay outside the camera math.

The accepted scene-wide implementation is a managed instance of the existing `arzoom_filter` attached directly to the OBS scene source after OBS has composed the scene.

## Accepted scene-wide architecture

```text
OBS Scene composition
        ↓
scene obs_source_t
        ↓ OBS normal filter chain
ArZoom Camera (managed arzoom_filter)
        ↓
Smart Camera + click/cursor presentation pass
        ↓
final scene output
```

This architecture is final for the current product line unless explicitly superseded by an architecture decision.

## Non-negotiable architecture invariants

1. **No persistent scene-item transform mutation.** Scene-wide zoom must not be implemented by writing position, scale, rotation, bounds, or crop to the user's composition.
2. **No duplicate scene-wide camera source.** Do not revive the rejected custom `ArZoom Camera` input-source/off-screen re-render experiment.
3. **One camera engine.** Per-source and scene-level modes reuse the accepted Smart Camera runtime.
4. **No invalid viewport exposure.** Supported zoom/follow math must not reveal invalid content.
5. **Local pointer motion may produce zero camera motion.** Mouse movement is not automatically camera movement.
6. **Frame-rate independence.** Equivalent time-based traces at 30, 60, 120, and 144 fps should converge to equivalent framing.
7. **Safe zoom-out near edges.** Re-centering and zoom-out remain coupled so widening the viewport cannot expose invalid edges.
8. **Idle pass-through remains available.** When no presentation effect is visually active, production filtering should continue using OBS pass-through where possible.
9. **No frame readback or per-frame file/settings writes.** These do not belong in the hot path.
10. **Presentation math stays independent of operating-system APIs.** Deterministic tests must run without OBS or a live desktop.
11. **Input capture and rendering remain separable.** Platform backends or GPU effects must not require rewriting camera policy.
12. **Presentation feedback is camera-isolated.** Click effects and Presentation Cursor cannot wake, retarget, or accelerate camera motion.

## Scene mapping contract

Scene-wide pointer-driven behavior depends on a proven mapping from desktop input coordinates to OBS scene coordinates.

The current v0.5.x safe case is intentionally conservative: one visible top-level fullscreen Display Capture with a deterministically resolved monitor.

Future support for scaled, inset, cropped, rotated, nested, or multi-capture layouts must extend the **read-only mapping layer**, not the render architecture.

Preferred approach:

- read capture/source geometry;
- read scene-item box/draw transforms;
- compose/invert transform chains;
- identify target ownership deterministically;
- reject ambiguous mappings with an explicit reason;
- never write transforms to make mapping easier.

## Accepted motion state model

```text
ACTIVATING
    ↓
SMOOTH_IDLE ↔ OBSERVE → FOLLOW / CATCH_UP → COAST → SMOOTH_IDLE
    ↓
RETURNING → REST
```

Presenter controls such as Freeze Camera, Smart Follow toggle, Zoom +/−, Reset/Full Frame, and Overview Peek operate on this same state model.

## Zoom and settle behavior

- Zoom trajectories are deterministic and minimum-jerk.
- Zoom-in preserves the chosen focus relationship.
- Zoom-out remains edge-safe.
- Completed idle states settle exactly instead of breathing through perpetual micro-corrections.
- Camera characters (Cinematic, Balanced, Responsive) tune the same model rather than selecting different implementations.

## Test seam

`tests/arzoom-trace-harness.hpp` and the phase regression suite provide platform-neutral replay/validation of camera behavior and architecture invariants.

When production behavior changes intentionally, tests and production logic must move together. Historical phase tests are regression contracts, not optional examples.

New scene-mapping work should add deterministic transform/mapping cases including failure cases for ambiguity.

## Hot-path performance contract

Camera/effect work should aim for:

- no frame readback;
- no per-frame file I/O;
- no per-frame OBS settings writes;
- no avoidable heap allocation inside camera update;
- compact bounded input/event state;
- true OBS pass-through while visually idle;
- a single presentation GPU pass when the architecture permits it;
- no private duplicate OBS scene render solely for Scene Camera.

Benchmarks are regression signals. Machine-specific timing values must not be used as universal marketing claims.

## Change control

Any proposal that requires scene-item writes, a second camera source/runtime, duplicated scene composition, CPU frame readback, or another violation of the invariants above must first update [`PROJECT_DIRECTION.md`](PROJECT_DIRECTION.md) with a reviewed rationale and migration/safety plan.

Do not treat old issues, abandoned experiments, or phase plans as authority over this contract.
