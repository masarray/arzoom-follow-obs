# Smart Gimbal Camera 2.0 — Tuning Gates

These are perceptual engineering gates for the Phase 1 public trial. They are not marketing claims.

## Core rule

A good ArZoom movement should be noticed for its result, not for the camera motion itself.

Priority order:

1. stability;
2. straight and seamless visual paths;
3. soft start / soft finish;
4. useful catch-up;
5. raw responsiveness.

## Straight screen-space zoom

Zoom-in and zoom-out are evaluated in the coordinate system the viewer actually sees, not only by the numeric camera center.

ArZoom interpolates the normalized screen transform:

```text
output = scale * source + offset
```

`scale` and `offset` share one quintic minimum-jerk progress value. Therefore any fixed source pixel must travel on one straight screen-space line from its initial rendered position to its final rendered position.

Release gates:

- no sideways bow/curve during zoom-in;
- no sideways bow/curve during zoom-out;
- no direction reversal on either axis;
- magnification is monotonic;
- zoom-in remains focus-preserving near edges/corners;
- zoom-out ends exactly at `1.0x` / `(0.5,0.5)`;
- 120 subsequent frames remain exactly stable.

## Explanation lock

Mouse movement is not automatically presenter relocation.

When the camera is settled, ArZoom arms a local explanation anchor around the current cursor area. The viewport remains stationary when the user:

- circles a button, diagram, word, or UI region;
- repeatedly points left/right or up/down inside the same local region;
- makes normal hand jitter around the current subject.

The lock uses both net displacement and path coherence. A circular or repeated gesture has high traveled path but low net displacement, while a real relocation leaves the local area with coherent net travel.

Edge safety always wins: if the pointer is genuinely close to being lost from the visible viewport, Smart Follow may leave the explanation lock.

## Activation focus continuity

- focus remains visible throughout zoom-in;
- edge/corner activation never detours toward unrelated center content;
- normal cursor jitter during activation does not retarget the latched shot;
- zoom and pan are one straight screen-transform transition.

## Viewer-comfort traces

- stationary jitter: zero camera displacement;
- repeated nearby pointing: effectively zero movement;
- explanation circle that slightly exceeds the legacy safe-zone: zero movement while still safely visible;
- after settle: exact lock with no residual breathing.

## Intent latency

Balanced starting target:

- ordinary candidate motion: roughly `80–130 ms` observation before a new follow shot;
- edge-risk may shorten this smoothly;
- future click/emphasis event may raise confidence without teleporting.

## Gimbal follow character

For a stationary new destination:

- first visible camera speed is very small;
- travel is monotonic toward destination;
- no overshoot;
- speed rises and falls gradually through cascaded filters;
- finish converges to exact stable lock.

The engine must not use a spring/ballistic braking cycle.

## Continuous retargeting

When the user moves the mouse again before arrival:

- no animation restart;
- no one-frame position jump;
- no instant direction reversal;
- current path bends progressively toward the new destination;
- repeated mouse updates do not create a stop/start cadence.

## Catch-up vs comfort

Edge urgency may make the **same gimbal filters** more responsive, but urgency itself must be smoothed. There is no alternate high-energy motion mode.

Balanced must preserve both:

1. ordinary presenter gestures do not move the viewport;
2. a long intentional relocation reaches useful framing before the cursor becomes visually lost.

## Default style intent

### Cinematic

Maximum stability and the largest explanation area. Best for teaching and detailed explanation.

### Balanced

Recommended default. Should feel like a stabilized motorized camera, not a mouse follower.

### Responsive

Smaller explanation area and shorter time constants for fast presentation while preserving no-overshoot, continuous-retarget behavior.

## Performance

Keep camera update fixed-state/O(1), allocation-free, with no frame readback, image analysis, or file/settings I/O in the hot path.

Performance regressions are investigated, but do not replace perceptually correct gimbal motion with visibly harsher movement to save negligible CPU time.

## Required OBS visual trial matrix

Test at minimum:

- 1080p60 Display Capture;
- center → every edge;
- center → every corner;
- edge → opposite edge;
- slow text explanation gesture;
- medium circle around a UI element, including a circle slightly outside the legacy safe-zone;
- repeated nearby pointing;
- mouse changes destination while camera is mid-travel;
- rapid direction reversal;
- mouse held at edge during zoom-in;
- zoom-out from each edge/corner, checking for any visible curved path;
- 2x, 3x, 4x;
- Cinematic / Balanced / Responsive;
- multi-monitor including negative desktop coordinates where available.

The final tuning decision is based on human visual comfort after deterministic gates are green.
