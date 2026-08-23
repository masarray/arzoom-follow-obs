# Smart Camera Motion 2.0 — Tuning Gates

This document turns Phase 1 behavior into observable release gates. Values are starting engineering targets and may be refined from public-trial evidence; they are not marketing claims.

## Activation focus continuity

For a zoom activation latched to a cursor focus point:

- the focus point must stay inside the visible output throughout the transition;
- its screen-space trajectory should move monotonically toward its final legal framing region rather than detouring toward unrelated center content;
- edge/corner activation must never reveal invalid source pixels;
- normal cursor jitter during activation must not continuously retarget the transition.

## Viewer-comfort traces

- stationary jitter: effectively zero camera displacement;
- repeated pointing inside one UI region: effectively zero camera displacement;
- small circular explanation gesture: target is materially lower motion than the v0.1.4 Phase 0 baseline (`0.004671` normalized max displacement);
- after SETTLE, camera velocity is exactly/near zero until intent crosses the re-entry threshold.

## Intent latency

Suggested starting ranges for Balanced mode:

- ordinary candidate motion: Observe window roughly `70–130 ms` before committed travel;
- high-urgency edge approach: shortened response;
- future click-at-new-location event: may immediately raise intent confidence without forcing a teleport.

The final values must be selected from replay traces and visual trials rather than hard-coded to these initial ranges.

## Ballistic character

- no instantaneous jump from rest to max pan speed;
- acceleration increases smoothly with bounded jerk;
- long travel may reach an urgency-scaled cruise/max speed;
- braking begins before target arrival;
- default motion should be critically or near-critically damped: visible bounce/oscillation is a failure;
- direction reversal must first shed incompatible velocity before accelerating strongly in the new direction.

## Catch-up vs comfort

Balanced mode must preserve both:

1. small explanation gestures do not cause frame wander;
2. a long intentional cursor relocation reaches useful framing before the presenter interaction is visually lost.

The intent system should raise urgency based on output-edge risk and distance rather than using one globally faster smoothing constant.

## Performance

Compare Phase 1 microbenchmark output against the recorded Phase 0 Windows baseline. Investigate any large regression in motion-core cost. The algorithm should remain fixed-state/O(1) with no heap allocation in the per-frame camera update.

## Required visual trial matrix

Test at least:

- 1080p60 Display Capture;
- cursor center → each edge;
- cursor center → each corner;
- edge → opposite edge;
- rapid direction reversal;
- slow text explanation gesture;
- small circle around a UI element;
- rapid pointing between nearby controls;
- mouse held at edge while zoom activates;
- 2x, 3x, 4x zoom;
- Smooth/Cinematic, Balanced, Responsive tuning profiles;
- multi-monitor including negative desktop coordinates where available.
