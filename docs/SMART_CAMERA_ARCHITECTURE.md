# ArZoom Smart Camera Architecture Contract

This document defines the stable vocabulary and engineering boundaries used by ArZoom's North Star roadmap. Phase 0 intentionally documents the current baseline before Smart Camera Motion 2.0 changes behavior.

## Product intent

ArZoom should behave like a skilled presentation camera operator, not a robotic mouse follower. Cursor movement is evidence of presenter intent, not a direct camera command.

## Domain language

| Term | Meaning |
|---|---|
| **Cursor Position** | Platform input sample mapped into normalized source coordinates. |
| **Presenter Intent** | Evidence that the presenter wants the audience to look somewhere else. Future versions derive this from distance, velocity, direction persistence, dwell, clicks, and edge urgency. |
| **Comfort Zone** | Region where normal hand jitter and explanatory pointer motion can occur without forcing camera movement. |
| **Follow Boundary** | Threshold outside the comfort zone where camera response can begin. Phase 1 will add explicit hysteresis between enter and exit thresholds. |
| **Camera Center** | Normalized source point at the center of the visible viewport. |
| **Camera Velocity** | Rate of change of Camera Center in normalized source units per second. |
| **Camera Acceleration** | Rate of change of Camera Velocity. Phase 1 will make acceleration and braking first-class motion concepts. |
| **Urgency** | How strongly the camera should prioritize catching up, for example when the cursor approaches a visible edge or relocates far away. |
| **Look Ahead** | Small, bounded framing bias in the direction of sustained cursor travel. |
| **Catch-up** | Fast but controlled travel used when the camera is meaningfully behind presenter intent. |
| **Brake** | Deceleration phase used as the camera approaches its intended framing. |
| **Settle / Lock** | Stable state where micro-corrections stop after the camera reaches an acceptable framing region. |
| **Edge Constraint** | Mathematical requirement that the visible viewport remains fully inside valid source coordinates. |
| **Click Event** | Timestamped, typed mouse click with normalized location. Clicks are presentation signals and later also drive GPU visualization. |
| **Presentation Event** | General future input event that can influence camera intent or presentation effects without coupling the camera core to a particular device. |

## Architecture boundaries

ArZoom separates four responsibilities:

```text
Platform Input
    ↓
Input Snapshot / Presentation Events
    ↓
Presenter Intent + Smart Camera Math
    ↓
GPU Presentation Rendering
```

### Platform input

Platform-specific code may read cursor position, buttons, monitor topology, DPI information, or future input devices. It must publish small normalized snapshots/events rather than embed camera policy.

### Camera math

Camera math must remain platform-independent and deterministic. It owns comfort-zone behavior, target framing, motion state, edge constraints, and future ballistic behavior.

### GPU presentation rendering

Rendering receives small camera/effect parameters. It must not depend on per-frame CPU frame readback. Future click pulses should be procedural GPU effects rather than generated image files or extra OBS scene items.

### OBS integration

OBS lifecycle, settings, hotkeys, source selection, and pass-through behavior stay outside the camera math. Default architecture should not require persistent mutation of user scene transforms.

## Phase 0 invariants

The following are non-negotiable regression contracts:

1. **No invalid viewport exposure.** Supported zoom/follow math must not reveal pixels outside the captured source.
2. **Small pointer motion may produce zero camera motion.** Mouse movement is not automatically camera movement.
3. **Frame-rate independence.** Equivalent time-based traces at 30, 60, 120, and 144 fps should converge to equivalent framing.
4. **Bounded apparent pan speed.** The v0.1.4 Smooth baseline remains capped by its configured apparent output-speed limit.
5. **Safe zoom-out near edges.** Re-centering and zoom-out remain coupled so widening the viewport cannot reveal an invalid edge.
6. **Idle pass-through remains available.** When no zoom/effect is visually active, the production filter must continue using OBS pass-through.
7. **No frame readback or per-frame file/settings writes.** Phase 0 must not introduce these into the hot path.
8. **Presentation math stays independent of operating-system APIs.** Deterministic tests must run without OBS or a live desktop.
9. **Input capture and rendering remain separable.** Future platform backends or GPU effects must not require rewriting the camera model.

## Phase 0 test seam

`tests/arzoom-trace-harness.hpp` is a deterministic replay model of the current v0.1.4 Smooth Smart Follow behavior. It exists to establish a baseline and a reusable trace vocabulary before Phase 1.

The harness is intentionally independent of OBS. It reuses `src/arzoom-math.hpp` and mirrors current orchestration constants. Phase 1 should either move the production camera orchestration into a shared platform-neutral core or update the harness and production core together so model drift cannot occur.

## Hot-path performance contract

For North Star development, camera/effect work should aim for:

- no frame readback;
- no per-frame file I/O;
- no per-frame OBS settings writes;
- no avoidable heap allocation inside camera update;
- compact input/event state;
- true OBS pass-through while visually idle;
- a single presentation GPU pass when the architecture permits it.

The Phase 0 microbenchmark is a directional regression signal. Absolute nanosecond values are machine-dependent and must not be used as marketing claims.
