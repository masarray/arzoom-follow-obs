# Smart Gimbal Camera Motion 2.0 — Phase 1 Spec

## North-star motion principle

ArZoom should feel like a well-operated gimbal: the viewport is extremely steady, moves only when presenter intent is clear, begins movement gently, follows one continuous path, and finishes so softly that the viewer barely notices the camera has moved.

The camera must optimize **viewer perception**, not simulate physical momentum.

## Explicit non-goals

The default camera path must not use:

- spring motion;
- ballistic acceleration/braking;
- bounce or overshoot;
- target chasing that restarts when the cursor moves again;
- predictive look-ahead that creates correction motion;
- frame-by-frame target collapse as the viewport approaches the mouse.

Ballistic motion was tested during the first Phase 1 trial and rejected because it produced visible correction/hunting, especially near the end of zoom-out and during continuous mouse retargeting.

## Architecture

```text
InputSnapshot
      ↓
Presenter Intent / Comfort Zone
      ↓
Stable Desired Focus
      ↓
Smoothed Moving Destination
      ↓
Cascaded Gimbal Servo
      ↓
Edge Constraint
      ↓
CameraOutput { zoom, center, state }
```

Zoom transitions use a separate coordinated minimum-jerk trajectory:

```text
latched start + latched destination
              ↓
     quintic minimum-jerk progress
              ↓
     zoom + center as ONE shot
```

The production OBS filter and deterministic tests must call the same shared `SmartCamera` core.

## Motion states

```text
REST → OBSERVE → FOLLOW / CATCH_UP → SETTLE → REST
  ↘ ACTIVATING
  ↘ RETURNING
```

`CATCH_UP` is not a different physics model. It only means the same gimbal filters are smoothly made more responsive by a filtered urgency signal.

## REST

- exact zero viewport movement;
- ordinary pointer jitter and explanation gestures inside the comfort region do nothing;
- all destination filter stages are locked to the current center so there is no hidden residual motion.

## OBSERVE

A cursor move outside the comfort region is not automatically a camera command. Accumulate intent from:

- distance outside the comfort region;
- dwell/persistence;
- direction persistence;
- output-edge risk;
- future semantic emphasis/click event.

No viewport movement occurs until intent is confirmed.

## FOLLOW — gimbal servo

Once intent is confirmed:

1. freeze a `follow_reference_center` for the current continuous shot;
2. derive a stable destination from cursor position relative to that fixed reference;
3. pass the destination through two low-pass destination stages;
4. pass the second destination stage through a slower camera low-pass stage;
5. clamp only to the legal center envelope.

For a stationary cursor, the destination must remain stationary. It must not collapse toward the current camera center while the camera is moving.

### Continuous retargeting

If the user moves the mouse again before the camera arrives, do **not** restart an animation and do not reset filter state. Change only the desired destination. The cascaded filters bend the existing path continuously toward the new destination.

Expected perception:

```text
old destination     new destination
      X ---------------- X
        \              /
         \ smooth bend /
          camera path
```

There must be no one-frame snap, instant direction reversal, or stop/start cadence.

## CATCH_UP

When cursor edge-risk or travel distance becomes high:

- filter the urgency value itself;
- shorten the same destination/camera time constants gradually;
- retain the same monotonic, no-overshoot gimbal character.

There is no switch to ballistic/spring motion.

## Zoom-in activation

Zoom activation is explicit presenter intent.

At activation:

1. latch current cursor as `activation_focus`;
2. latch start center and start zoom;
3. calculate the legal final framing;
4. use a quintic minimum-jerk progress curve;
5. coordinate zoom and center with the same progress;
6. preserve the visual relevance of the latched focus throughout the transition;
7. ignore normal cursor jitter until activation finishes;
8. hand off to normal Smart Follow only after the shot is stable.

Minimum-jerk progress:

```text
p(t) = 10t^3 - 15t^4 + 6t^5
```

It has zero velocity and zero acceleration at both endpoints.

Critical invariant: an edge/corner cursor must never produce `zoom center → then pan to cursor` behavior.

## Zoom-out / return

Zoom-out must be one coordinated cinematic shot, not a camera follow operation.

At toggle-off:

1. latch `return_start_center` and `return_start_zoom`;
2. destination is exactly `{ center: (0.5, 0.5), zoom: 1.0 }`;
3. advance one quintic minimum-jerk progress value;
4. interpolate both center and zoom with that same progress;
5. keep the legal viewport constraint as a numerical safety guard only;
6. finish at exact 1x / exact canvas center;
7. hard-lock all residual filter state to zero.

Required visual behavior:

- center distance to `(0.5,0.5)` is monotonic;
- zoom magnitude is monotonic;
- no late sideways correction;
- no overshoot;
- no final wobble;
- no breathing after settle.

## No predictive look-ahead by default

The initial Phase 1 trial showed that prediction can create unnecessary correction when a user changes speed/direction. Stability has higher priority than anticipation.

Future optional look-ahead may be reconsidered only if it can pass perceptual A/B tests without adding visible correction motion.

## Default tuning

Basic UI remains:

- `Cinematic`
- `Balanced` — recommended default
- `Responsive`

All three use the same motion model; only time constants and intent delay differ.

`Cinematic` prioritizes maximum steadiness.
`Balanced` should already feel gimbal-like and subtle for normal tutorials.
`Responsive` catches up sooner but must remain smooth and no-overshoot.

## Deterministic acceptance gates

### Activation

- center/edges/corners at 2x/3x/4x;
- focus remains visible;
- camera trajectory never detours away from intended focus;
- zero invalid viewport exposure.

### Viewer comfort

- normal jitter: zero movement;
- small explanation circle: zero movement;
- repeated nearby pointing inside comfort region: effectively zero movement.

### Stationary intentional relocation

- delayed intent observation before movement;
- very small first visible speed;
- monotonic travel toward destination;
- no overshoot;
- gradual speed decay;
- exact steady lock.

### Moving destination

- retargeting produces no one-frame position jump;
- direction change bends over multiple frames rather than reversing instantly;
- no stop/start/relaunch behavior;
- final framing converges and locks.

### Zoom-out

- center distance and zoom are monotonic for every frame;
- no directional reversal or sideways hunting;
- soft start;
- exact full-frame finish;
- at least 120 subsequent frames show zero camera breathing.

### Frame-rate invariance

Equivalent traces at 30/60/120/144 fps must converge within defined tolerances.

## Performance contract

Per-frame camera math remains fixed-state/O(1) with no heap allocation, frame readback, image analysis, per-frame settings writes, or file I/O.

Performance is measured, but perceptual steadiness is the primary Phase 1 objective as long as camera math remains negligible relative to an OBS video-frame budget.

## Exit gate

Phase 1 is complete only when:

- center-detour zoom-in bug is eliminated;
- zoom-out has zero visible final wobble;
- follow feels like one smooth gimbal glide rather than mouse chasing;
- mid-flight retargeting bends continuously;
- ordinary presenter gestures remain stationary;
- settle produces exact zero micro-movement;
- edge safety remains zero-violation;
- Windows tests/build/installer packaging are green;
- human OBS trial confirms the movement is comfortable to watch.
