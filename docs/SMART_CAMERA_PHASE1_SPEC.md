# Smart Zone Gimbal Camera — Phase 1 Final Contract

## North-star motion principle

ArZoom follows **presentation-area changes**, not every mouse movement.

The viewport should feel like a well-operated motorized gimbal: extremely steady while the presenter explains one area, soft and seamless when a real relocation happens, and visually quiet enough that the viewer notices the subject rather than the camera.

## Explicit non-goals

The default camera path must not use:

- spring or ballistic motion;
- bounce, overshoot, or braking oscillation;
- frame-by-frame mouse chasing;
- predictive look-ahead that creates correction motion;
- visible mode switches between moving and steady;
- a requirement for the mouse to stop before the camera may become steady.

The early ballistic Phase 1 experiment was rejected after visual trial because it produced correction/hunting and perceptible stop behavior.

## Final architecture

```text
InputSnapshot
      ↓
Presenter Area / Smart Zone Intent
      ↓
Observe / relocation confirmation
      ↓
Stable Moving Destination
      ↓
Cascaded Gimbal Servo
      ↓
Coast handoff
      ↓
SmoothIdle anchored presentation zone
      ↓
Edge Constraint
      ↓
CameraOutput
```

Zoom-in and zoom-out use a separate coordinated affine-transform trajectory:

```text
latched start transform
        ↓
quintic minimum-jerk progress
        ↓
latched final transform
```

The production OBS filter and deterministic tests use the same shared `SmartCamera` implementation.

## Final motion states

```text
ACTIVATING
    ↓
SMOOTH_IDLE ↔ OBSERVE → FOLLOW / CATCH_UP → COAST → SMOOTH_IDLE
    ↓
RETURNING → REST
```

`CATCH_UP` is not different physics. It only shortens the same gimbal time constants through a filtered urgency signal.

## SmoothIdle / Smart Zone

`SmoothIdle` is the default steady presentation state while zoomed.

When active:

- viewport position is exact-locked;
- local circles, repeated pointing, and ordinary explanation gestures do not move the camera;
- no destination cascade is advanced while the pointer remains local;
- the presenter does not need to stop moving the mouse;
- an inner/outer zone hysteresis prevents Follow/Idle chatter near one threshold.

A new follow shot begins only after the pointer meaningfully leaves the outer presentation zone for a short intent dwell, unless edge risk or a semantic emphasis event makes the relocation urgent.

## Observe

A cursor leaving the local Smart Zone is not immediately a camera command.

Observe accumulates relocation confidence from:

- displacement from the presentation zone;
- short dwell/persistence;
- direction persistence;
- output-edge risk;
- semantic emphasis/click events.

No camera movement occurs until relocation is confirmed.

## Follow / CatchUp

Once relocation is confirmed:

1. preserve a stable reference for the continuous follow shot;
2. derive a destination from the new presenter area;
3. pass destination through cascaded low-pass gimbal stages;
4. retain filter state when the mouse retargets mid-flight;
5. smoothly raise responsiveness only when edge risk or travel distance requires it.

A moving destination bends the existing path; it does not restart animation.

Required character:

- soft first movement step;
- no one-frame snap;
- no overshoot;
- no stop/start cadence;
- no instant direction reversal.

## Coast handoff

The camera must not snap from Follow to steady.

As soon as the pointer has been reacquired into a useful new presentation area, ArZoom enters `Coast` even if the presenter immediately starts circling or pointing there.

During Coast:

- live-pointer influence fades progressively;
- the current gimbal path continues smoothly;
- camera speed decays to a visually negligible value;
- the final presentation-zone anchor is established;
- only then does the state become `SmoothIdle`.

The transition must be visually continuous; the viewer should not perceive a mode change.

## Straight screen-space zoom

Zoom-in and zoom-out interpolate the affine screen transform:

```text
output = scale * source + offset
```

`scale` and `offset` share the same quintic minimum-jerk progress value:

```text
p(t) = 10t^3 - 15t^4 + 6t^5
```

This guarantees that every fixed source pixel travels along a straight screen-space line between start and finish while velocity and acceleration are zero at both endpoints.

### Zoom-in

- latch the activation focus;
- calculate the legal final framing;
- preserve focus relevance from the first visible zoom frame;
- do not detour through unrelated center content;
- ignore normal cursor jitter until activation completes.

### Zoom-out

- latch the current screen transform;
- destination is exact full-frame transform;
- use one monotonic minimum-jerk shot;
- no sideways curve, correction, overshoot, or wobble;
- finish at exact `1.0x` and `(0.5, 0.5)`;
- subsequent frames remain exact-locked.

## Styles

All styles use the same architecture:

- `Cinematic` — largest steady zone and slowest/softest movement;
- `Balanced` — recommended default;
- `Responsive` — shorter dwell/time constants while retaining the same no-snap behavior.

Persisted legacy values remain compatible.

## Phase 1 deterministic gates

Required gates include:

- existing Phase 0 edge/math regressions;
- straight screen-space zoom-in and zoom-out;
- edge/corner activation at 2x/3x/4x;
- local explanation circle remains stationary;
- relocation enters Follow and hands off through Coast;
- presenter may keep moving the mouse during arrival while camera reaches SmoothIdle;
- speed is already near zero before exact idle lock;
- SmoothIdle remains pixel-stable;
- leaving the outer zone wakes a new follow with a soft first movement step;
- continuous retargeting has no one-frame snap;
- rapid zone switching remains bounded and edge-safe;
- Smart Zone matrix across 2x/3x/4x and 30/60/120/144 fps;
- corner zoom-out matrix finishes at exact full-frame lock.

## Performance contract

Per-frame camera logic remains fixed-state/O(1), allocation-free, with no frame readback, image analysis, per-frame file/settings I/O, or growing history containers.

`SmoothIdle` is intentionally a cheap fast path because the destination cascade does not need to advance while the presenter remains in one area.

## Phase 1 exit gate

Phase 1 is complete when:

- zoom-in is focus-preserving and straight;
- zoom-out is straight and wobble-free;
- long relocation is smooth and continuous;
- explanation gestures after relocation remain steady;
- Follow → Coast → SmoothIdle is not perceptibly abrupt;
- SmoothIdle → new Follow launches softly;
- edge safety remains zero-violation;
- stress matrix and frame-rate gates pass;
- Windows plugin + installer + manual ZIP build successfully;
- human OBS trial confirms the motion is comfortable to watch.
