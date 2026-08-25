# ArZoom Smart Camera Architecture Contract

**Status:** current engineering contract for the accepted v0.6.0 Smart Camera, Scene Camera motion, and presentation-rendering model.

Read [`PROJECT_DIRECTION.md`](PROJECT_DIRECTION.md) and [`P4_1_STABLE_BASELINE.md`](P4_1_STABLE_BASELINE.md) first. P4.1 stable-baseline behavior takes precedence over older phase tuning notes.

## Product intent

ArZoom should behave like a skilled presentation camera operator, not a robotic mouse follower. Cursor movement is evidence of presenter intent, not a direct camera command.

The accepted behavior keeps local explanation work calm, reacts promptly to meaningful relocation, keeps pointer context acquired, moves with continuous kinematics, and settles exactly without repeated searching.

## Domain language

| Term | Meaning |
|---|---|
| **Cursor Position** | Platform input sample mapped into the active presentation coordinate space. |
| **Presenter Intent** | Evidence that the presenter wants the audience to look somewhere else. |
| **Useful Context Envelope** | Region where pointer context is still acceptably framed and local work can remain calm. |
| **Hard Visibility Envelope** | Outer safety region used to prioritize pointer reacquisition before prolonged loss. |
| **Camera Center** | Point at the center of the visible viewport in the active camera coordinate space. |
| **Camera Velocity** | Rate of change of Camera Center. |
| **Camera Acceleration** | Rate of change of Camera Velocity. |
| **Jerk** | Rate of change of Camera Acceleration; bounded in P4.1 motion. |
| **Follow Pressure** | Continuous urgency signal describing how strongly the camera should prioritize catch-up. |
| **Tracking Latch** | Hysteresis state that prevents follow from chattering on/off around one threshold. |
| **Exact HOLD** | Final drift-free state with zero camera motion until new intent appears. |
| **Presentation Cursor** | ArZoom-rendered cursor/pointer feedback in the presentation pass. |

## Architecture boundaries

ArZoom separates responsibilities:

```text
Platform Input / OBS Commands
        ↓
Input Snapshot / Presentation Events
        ↓
Read-only Mapping
        ↓
SceneViewportPlanner — semantic WHERE
        ↓
SceneKinematicMotion — physical HOW
        ↓
OBS Integration + GPU Presentation Rendering
```

### Platform input

Platform-specific code may read cursor position, buttons, monitor topology, DPI information, or future input devices. It publishes compact snapshots/events rather than embedding camera policy.

### Read-only scene mapping

Mapping proves desktop/capture/scene coordinate ownership without writing scene transforms. v0.6.0 supports one visible top-level Display Capture with deterministic monitor ownership and proven positive axis-aligned fullscreen/scale/inset/crop mapping.

### Semantic camera planner

`SceneViewportPlanner` owns presentation-context decisions: whether movement is warranted, where the pointer should land contextually, tracking latch state, follow pressure, prediction policy, and edge-safe target framing.

Pressure/urgency may change **HOW** decisively motion proceeds; it must not create a feedback loop that continuously changes **WHERE** the pointer target corridor should land.

### Kinematic motion synthesizer

`SceneKinematicMotion` owns physical motion continuity using explicit position, velocity, and acceleration state. Acceleration changes are jerk-limited using bounded substeps.

The same motion state survives live tracking → final settle. Moving targets may use bounded far-distance cruise authority; effectively immutable targets transition to jerk-aware precision braking without resetting velocity/acceleration. Final completion enters exact HOLD.

This synthesizer is not a second Smart Camera policy engine: it has no scene ownership, pointer semantics, or OBS mutation authority.

### GPU presentation rendering

Rendering receives compact camera/effect parameters. It must not depend on CPU frame readback. Click feedback and Presentation Cursor belong in the presentation output path and use bounded state.

### OBS integration

OBS lifecycle, settings, hotkeys, filter ownership, scene selection, mapping validation, and pass-through behavior stay outside semantic camera policy.

The accepted scene-wide implementation is a managed instance of the existing `arzoom_filter` attached directly to the OBS scene source after normal scene composition.

## Non-negotiable architecture invariants

1. **No persistent scene-item transform mutation.** Scene-wide zoom must not write position, scale, rotation, bounds, or crop to the user's composition.
2. **No duplicate scene-wide camera source.** Do not revive the custom `ArZoom Camera` input-source/off-screen re-render experiment.
3. **One semantic camera authority.** Do not add a competing Smart Camera/planner beside `SceneViewportPlanner`.
4. **Kinematic motion may be a separate layer, not a separate policy engine.** It only realizes already-selected targets.
5. **No invalid viewport exposure.** Supported zoom/follow math must not reveal invalid content.
6. **Local pointer motion may produce zero camera motion.** Mouse movement is not automatically camera movement.
7. **Pointer acquisition and motion quality are co-equal.** Do not improve one by regressing the other.
8. **Frame-rate independence.** Equivalent time-based traces at 30, 60, 120, and 144 fps should converge to equivalent framing.
9. **Idle pass-through remains available.** When no presentation effect is visually active, production filtering should continue using OBS pass-through where possible.
10. **No frame readback or per-frame file/settings writes.** These do not belong in the hot path.
11. **Presentation math stays independent of OS APIs.** Deterministic tests must run without OBS or a live desktop.
12. **Presentation feedback is camera-isolated.** Click effects and Presentation Cursor cannot wake, retarget, or accelerate semantic camera intent.

## P4.1 mapping contract

v0.6.0 supports pointer-driven scene behavior only when mapping can be proven:

- exactly one visible top-level Display Capture;
- deterministic monitor ownership;
- positive axis-aligned fullscreen/scale/inset placement;
- crop-aware mapping when deterministic;
- shared mapped coordinates for Smart Follow, click anchoring, and Presentation Cursor;
- exact live camera zoom controls Presentation Cursor magnification.

Unsupported/ambiguous cases fail safe rather than guess: multiple candidate Display Captures, unproven rotation/skew/flips, unsupported bounds modes, unresolved nested ownership, or invalid geometry.

Future support must extend the read-only mapping layer, not the render architecture.

## Accepted P4.1 motion model

The old phase-state vocabulary remains useful for product semantics, but scene-wide physical movement now follows this contract:

```text
CALM / HOLD
    ↓ meaningful context exit
TRACK (latched, continuous pressure)
    ↓ moving target
KINEMATIC CRUISE / CATCH-UP
    ↓ pointer intent stabilizes
FINAL TARGET
    ↓ same velocity/acceleration state
JERK-AWARE PRECISION BRAKING
    ↓
EXACT HOLD
```

Explicit Zoom In/Out and Reset/Return keep their deterministic focus/edge-safe trajectories and must not reintroduce freezing or harsh stop/restart behavior.

## Prediction contract

Pointer prediction is allowed only as a bounded lag-reduction aid:

- short lead time;
- confidence-gated;
- displacement-capped;
- confidence cleared on pointer direction reversal;
- disabled/decayed when the pointer settles;
- must not convert local explanation gestures into continuous chasing.

## Test seam

The phase regression suite is a product contract, not an optional example. P4.1 adds explicit deterministic gates for:

- generalized mapping and fail-safe ambiguity;
- high-zoom final framing;
- bounded pointer-loss intervals at 2× and 4×;
- fixed-target convergence;
- bounded jerk;
- no zero-speed stall while meaningful target travel remains;
- retarget velocity continuity;
- bounded intentional direction reversal;
- continuous follow pressure;
- tracking chatter/entry bounds;
- no stop-start planner stalls;
- exact HOLD with zero drift and no target regeneration.

Do not weaken a gate merely to make a new implementation pass. Intentional product changes require documented rationale, replacement contract, deterministic evidence, and direct OBS trial when motion is user-visible.

## Hot-path performance contract

Camera/effect work should aim for:

- O(1) bounded state;
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

Any proposal that weakens pointer acquisition, reintroduces stutter/searching, breaks exact HOLD, requires scene-item writes, adds a second semantic camera source/runtime, duplicates scene composition, or uses CPU frame readback must first update [`PROJECT_DIRECTION.md`](PROJECT_DIRECTION.md) and [`P4_1_STABLE_BASELINE.md`](P4_1_STABLE_BASELINE.md) with reviewed rationale and migration/safety evidence.
