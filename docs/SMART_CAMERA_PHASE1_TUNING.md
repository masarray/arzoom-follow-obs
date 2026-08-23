# Smart Zone Gimbal Camera — Phase 1 Closeout Gates

These are perceptual engineering gates for the v0.2.0 public trial.

## Core rule

ArZoom follows **presentation-area changes**, not ordinary pointer motion.

Priority order:

1. stability;
2. seamless Follow → Coast → SmoothIdle handoff;
3. straight zoom paths;
4. soft launch / soft finish;
5. timely relocation;
6. raw responsiveness.

## SmoothIdle

When the presenter remains in one explanation area:

- viewport must be exact-stable;
- circles, repeated pointing, and local hand motion do not move the camera;
- the mouse does not need to stop;
- destination filters do not advance unnecessarily;
- no camera breathing is allowed.

## Smart Zone hysteresis

Use separate local/exit semantics:

- local explanation motion stays inside SmoothIdle;
- crossing the outer zone begins Observe, not an immediate pan;
- short dwell confirms a real area relocation;
- edge risk or semantic emphasis may shorten the dwell;
- boundary hovering must not chatter between idle and follow.

## Coast handoff

Follow must never snap to steady.

Release gates:

- camera enters Coast after the new area is usefully reacquired;
- presenter may immediately move/circle the mouse during Coast;
- live-pointer influence fades rather than disappearing in one frame;
- camera speed is already visually negligible before exact SmoothIdle lock;
- there is no visible stop event when the state changes.

## SmoothIdle wake-up

Leaving the outer Smart Zone must start another follow shot with:

- short intent dwell;
- very small first visible movement step;
- preserved destination-filter continuity;
- no teleport, snap, or instant cruise-speed launch.

## Straight screen-space zoom

Zoom-in/out are tested in viewer coordinates:

```text
output = scale * source + offset
```

`scale` and `offset` share one quintic minimum-jerk progress value.

Required:

- no sideways bow or curve;
- no direction reversal;
- monotonic magnification;
- focus-preserving zoom-in at edges/corners;
- zoom-out exact-locks to `1.0x / (0.5,0.5)`;
- post-return frames remain exact-stable.

## Continuous retargeting

During a real follow shot:

- moving the mouse again changes the destination without restarting animation;
- no one-frame jump;
- no abrupt reversal;
- path bends progressively;
- repeated updates do not create a stop/start cadence.

## Styles

### Cinematic
Largest steady zone, longest Coast, softest travel.

### Balanced
Recommended default. Strong presentation-area stability with useful catch-up.

### Responsive
Smaller steady zone and shorter dwell/time constants, but still no snap or alternate physics.

## Automated closeout matrix

CI must cover:

- 30/60/120/144 fps;
- 2x/3x/4x;
- repeated relocations across multiple presentation zones;
- local explanation orbit after each settle;
- rapid zone switching;
- center/edge/corner safety;
- corner zoom-out at all supported zoom levels;
- exact full-frame lock after return;
- existing Phase 0 math and edge invariants.

## Performance

The camera core remains fixed-state/O(1), allocation-free, with no frame readback, image analysis, or file/settings I/O in the hot path.

Hosted-runner nanosecond measurements are diagnostics only. A small math-cost increase is acceptable when it materially improves stability, provided the cost remains negligible relative to an OBS frame budget.

## Human closeout trial

Before Phase 1 is considered final, verify in OBS:

- long mouse relocation feels smooth;
- presenter can start circling immediately after relocation without viewport chasing;
- Follow → steady cannot be visually identified as a mode switch;
- leaving the current zone launches another smooth follow;
- zoom-in/out remain straight and quiet;
- no visible edge exposure or post-settle breathing.
