# Smart Camera Motion 2.0 — Phase 1 Spec

## Problem statement

ArZoom v0.1.4 is edge-safe and jitter-resistant inside its safe zone, but it is not yet a smart camera. Its current Smart Follow is a dead-zone correction model: the camera target changes only when the cursor crosses a boundary, while camera movement uses position-only exponential smoothing plus a maximum speed clamp.

This creates two user-visible problems:

1. **Robotic follow** — there is no persistent camera velocity, acceleration, braking, jerk limiting, intent confidence, hysteresis, or settle state.
2. **Wrong zoom-in framing near edges** — when activation begins at 1x, the only legal camera center is `(0.5, 0.5)`. Zoom and pan are then advanced independently, so the first visible zoom frames the screen center before pan catches up to a cursor near an edge. The user perceives this as zooming into empty content before finding the intended subject.

Phase 1 must fix the model, not just retune constants.

## Product contract

ArZoom should behave like a skilled camera operator:

- ordinary hand jitter and explanatory pointer movement do not move the camera;
- an intentional relocation is recognized quickly;
- zoom-in begins around the intended focus instead of detouring through screen center;
- long travel accelerates smoothly, catches up decisively, brakes before arrival, and settles completely;
- cursor approach to an output edge raises urgency before the event is lost;
- direction reversal does not cause oscillation or robotic snapping;
- all supported motion remains edge-safe;
- runtime work remains O(1), allocation-free in the camera hot path, and platform-independent.

## Architecture

Separate camera policy from OBS integration:

```text
InputSnapshot / PresentationEvent
        ↓
IntentEstimator
        ↓
CameraPlanner
        ↓
BallisticIntegrator
        ↓
EdgeConstraint
        ↓
CameraOutput { zoom, center, state }
```

The production filter should call this shared core. The deterministic Phase 0 harness should replay the same core so tests cannot drift from runtime behavior.

## Runtime state machine

```text
REST → OBSERVE → FOLLOW → CATCH_UP → BRAKE → SETTLE → REST
  ↘──────── activation / high-confidence event ────────↗
```

### REST
Camera is stationary. Cursor movement inside the comfort region has no effect.

### OBSERVE
A candidate relocation is detected. Accumulate confidence from displacement, velocity, direction persistence, dwell, and future click events. Small circular gestures should normally decay back to REST.

### FOLLOW
Intent is confirmed. Camera begins controlled motion with bounded acceleration.

### CATCH_UP
Used for large relocation or edge risk. Max velocity/acceleration may increase with urgency, but jerk and edge constraints remain bounded.

### BRAKE
Begin deceleration before the desired framing point so the camera does not overshoot or feel like a spring toy.

### SETTLE
When position error and velocity are both small, stop micro-corrections and lock framing. Hysteresis prevents immediate re-entry into FOLLOW.

## Focus-preserving zoom activation

Zoom activation is a dedicated transition and must not use the normal follow algorithm unchanged.

At activation:

1. sample and latch the current normalized cursor as `activation_focus`;
2. compute the legal final framing for the configured zoom;
3. drive zoom with a smooth monotonic activation curve;
4. derive a legal center trajectory from zoom progress so framing moves toward the focus *as zoom opens the available pan range*;
5. do not allow ordinary cursor jitter during the activation window to retarget the camera;
6. allow a high-confidence relocation/click to supersede the latched focus;
7. hand off to Smart Follow only after activation has reached a stable camera state.

A simple acceptable formulation is to interpolate from the full-frame center to the final focus center using a smoothstep-like progress curve, then clamp the result to the legal center envelope for the current zoom. The center transition should be tied to zoom progress rather than independently lagging behind it.

Important UX invariant:

> If the cursor is near a source edge when zoom is triggered, the content under/near that cursor should remain visually relevant from the first visible zoom frame. The camera must not visibly zoom into unrelated center content first.

## Intent estimator

Keep the estimator deterministic and cheap. Candidate signals:

- cursor displacement relative to comfort region;
- cursor speed;
- direction persistence over a short rolling state;
- distance from the visible output edge;
- dwell near a new location;
- direction reversal;
- typed click event (future Phase 2 visual event, but already a useful intent signal).

Use a small fixed-size/state representation; no heap allocation, history vectors, AI, CV, or frame analysis.

Suggested concept:

```text
intent_confidence =
    distance_weight +
    persistent_velocity_weight +
    edge_urgency_weight +
    dwell_weight +
    click_boost -
    reversal_penalty -
    gesture/jitter_decay
```

Exact constants must be tuned through deterministic traces rather than exposed directly to users.

## Ballistic camera integrator

Replace position-only smoothing with persistent state:

```text
camera_position
camera_velocity
camera_acceleration
```

Recommended model: critically/near-critically damped target acceleration with explicit velocity, acceleration, and jerk limits.

Conceptually:

```text
position_error = target - position
raw_accel = kp * position_error - kd * velocity
accel = jerk_limit(previous_accel, raw_accel, dt)
velocity += accel * dt
velocity = clamp_magnitude(velocity, max_speed * urgency_scale)
position += velocity * dt
```

Then apply edge constraints. When an axis hits a legal center boundary, remove velocity/acceleration components that continue pushing outside the valid viewport.

Urgency should scale limits smoothly, not switch between abrupt presets.

## Look-ahead

Use a small, bounded look-ahead based on sustained cursor velocity, not instantaneous mouse delta.

- decays rapidly when cursor slows;
- suppresses or reverses safely on direction change;
- bounded so the pointer never causes excessive framing drift;
- disabled/near-zero while intent confidence is low.

## Comfort zone and hysteresis

Use separate thresholds for entering movement and returning to rest.

Example concept:

- inner comfort region: free explanatory movement;
- outer intent boundary: candidate movement begins;
- settle boundary: camera may stop while the cursor is still outside the exact center.

This avoids move/stop/move chatter around one threshold.

## Urgency

Urgency increases when:

- cursor is close to or beyond the visible output safe boundary;
- distance to intended target is large;
- a high-confidence event occurs at a new location.

Urgency may increase acceleration and max speed, but should not bypass jerk limits or edge safety.

## Default product tuning

Basic UI should remain simple:

- Follow Style: `Cinematic / Balanced / Responsive`
- Camera Stability

Advanced values may expose safe-zone and expert controls later, but the default profile must be excellent without tuning.

`Cinematic` should favor viewer comfort and strongest gesture rejection.
`Balanced` should be the recommended default.
`Responsive` should prioritize faster catch-up while retaining ballistic motion.

## Phase 1 deterministic acceptance tests

Re-use all Phase 0 traces and add activation-specific traces.

### P1 activation regression

- start at 1x with cursor center, left edge, right edge, top edge, bottom edge, and four corners;
- trigger zoom to 2x, 3x, and 4x;
- assert zero invalid viewport exposure;
- measure screen-space distance between the initial focus and the visible framing during the transition;
- reject any trajectory that first biases toward unrelated center content before moving toward the latched focus.

### Jitter / explanation gesture

- normal jitter: camera displacement effectively zero;
- repeated pointing inside one UI region: no visible camera shake;
- small explanation circle: materially less camera displacement than v0.1.4 baseline.

### Intentional relocation

- nearby deliberate motion does not overreact;
- long relocation enters FOLLOW/CATCH_UP and arrives without losing the event;
- velocity grows smoothly rather than hitting max speed instantly;
- braking begins before arrival;
- settle leaves velocity at zero without oscillation.

### Reversal

- reversal must not overshoot wildly;
- look-ahead must collapse/reverse safely;
- camera must avoid high-frequency left-right oscillation.

### Edge/corner

- zero invalid source exposure;
- urgency increases before cursor becomes visually lost;
- velocity components into a clamped boundary are cancelled cleanly.

### Frame-rate invariance

Equivalent traces at 30/60/120/144 fps must remain perceptually and numerically close under tolerances defined by the test suite.

## Performance contract

Phase 1 camera update remains O(1) per frame and should preserve the Phase 0 performance class.

No:

- frame readback;
- image analysis;
- per-frame settings access/writes;
- file I/O;
- heap allocation in camera update;
- container growth in hot path.

Benchmark must compare Phase 1 against the recorded Phase 0 Windows baseline. Absolute hosted-runner numbers are diagnostic only; large regressions require investigation.

## Delivery order

1. Extract/share production camera orchestration core with tests.
2. Add failing activation regression reproducing center-detour bug.
3. Implement focus-preserving activation trajectory.
4. Add explicit camera state/velocity/acceleration.
5. Implement ballistic integrator + edge velocity handling.
6. Add intent estimator + hysteresis.
7. Add urgency and restrained look-ahead.
8. Tune Cinematic/Balanced/Responsive against traces.
9. Benchmark against Phase 0.
10. Windows CI + package public trial build.

## Exit gate

Phase 1 is complete only when:

- the reported center-detour activation bug is eliminated;
- ordinary explanatory mouse movement produces essentially stationary framing;
- intentional relocation is caught quickly enough for presentation use;
- acceleration/braking are visibly smooth and non-robotic;
- camera settles fully without micro-movement;
- edge safety remains zero-violation across deterministic/stress tests;
- Windows CI and packaging are green;
- benchmark shows no unacceptable hot-path regression.
