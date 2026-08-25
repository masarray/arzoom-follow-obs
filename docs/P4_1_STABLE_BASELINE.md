# ArZoom P4.1 Stable Baseline — v0.6.0

**Status:** ACCEPTED STABLE BASELINE. This contract records the direct-OBS accepted behavior that ships as ArZoom v0.6.0. Future work may improve it, but must not silently regress it.

Read `PROJECT_DIRECTION.md` first. When older phase documents conflict with this file on P4.1 mapping or Scene Camera motion, this file takes precedence.

## Release acceptance

P4.1 Trial 8 was accepted after direct OBS use on 2026-08-25. The accepted candidate was branch `phase4.1/read-only-scene-mapping` at commit `14395493e53d7d477e3e44ff5153d5b5d4a28a40` before release-lock documentation/version changes.

Windows CI #184 passed the complete deterministic suite, motion benchmark, actual OBS plugin build, Windows packaging, and artifact upload on that exact runtime head.

The release-lock changes after that accepted runtime head must not change Scene Camera runtime/motion behavior.

## Product character that is now locked

ArZoom Scene Camera is a presentation camera, not a robotic mouse follower.

The accepted behavior is:

- local pointer work remains calm;
- when the pointer leaves useful viewport context, the camera begins following promptly;
- far movement is decisive enough to keep the pointer acquired;
- acceleration and direction changes remain visually smooth;
- no stop → restart → stop cadence while meaningful travel remains;
- no repeated left/right or up/down searching after the final pointer location is known;
- the final pointer lands in a useful near-centre contextual area rather than being forced to exact centre;
- after final framing, the viewport becomes exactly still;
- behavior remains useful at both normal and high zoom.

A future change that makes pointer acquisition better but visibly reintroduces stutter/searching is a regression. A future change that makes motion prettier but allows long pointer loss is also a regression. Both qualities are required together.

## One camera authority: WHERE vs HOW

Managed Scene Camera keeps one semantic camera authority with two separated responsibilities:

```text
mapped presentation input
        ↓
SceneViewportPlanner
        ↓ decides WHERE the viewport should frame
SceneKinematicMotion
        ↓ decides HOW the viewport physically moves there
camera output
```

`SceneKinematicMotion` is a motion synthesizer, not a second Smart Camera policy engine. It has no scene ownership, pointer semantics, presentation-zone policy, or OBS mutation authority.

### Planner owns WHERE

The planner owns:

- presentation-context / optimal framing decisions;
- Smart vs Centered vs Fixed follow policy;
- pointer settle and tracking intent;
- continuous follow pressure;
- conservative pointer prediction policy;
- target framing and edge-safe center constraints.

Pressure/urgency may change motion authority, but must not continuously move the desired landing corridor. The same input context should not create a target feedback loop where camera motion itself changes WHERE the planner wants to land.

### Kinematic synthesizer owns HOW

The motion layer owns explicit:

- position;
- velocity;
- acceleration.

Acceleration changes are jerk-limited using bounded fixed substeps. Meaningful velocity/acceleration state survives live tracking → final settle; there is no artificial zero-velocity restart at the handoff.

Moving viewport targets may use bounded far-distance cruise authority so the camera can keep pace. When the target becomes effectively immutable, the same motion state transitions to conservative jerk-aware precision braking. Final settle ends in exact drift-free HOLD.

## Tracking / prediction contract

Tracking must be stable rather than threshold-chattering:

- tracking uses entry/exit hysteresis / latch behavior;
- follow pressure is continuous from useful context toward the visibility edge;
- high zoom may start tracking earlier because the physical viewport is smaller;
- far distance may increase HOW decisively the camera moves;
- pointer prediction stays short, confidence-gated, and displacement-capped;
- prediction confidence is cleared on pointer direction reversal;
- prediction must not turn local explanation gestures into continuous cursor chasing.

## P4.1 read-only scene mapping contract

v0.6.0 extends the safe Scene Camera mapping beyond the old fullscreen-only v0.5.x case.

Supported P4.1 ownership is deliberately narrow and deterministic:

- exactly one visible top-level Display Capture owns the presentation mapping;
- captured monitor identity must resolve deterministically;
- positive axis-aligned placement is supported;
- scaled/inset placement is supported when the transform can be proven;
- crop-aware mapping is supported when the transform can be proven;
- Smart Follow, click anchoring, and Presentation Cursor use the same mapped coordinate path;
- Presentation Cursor size follows the exact live camera zoom.

Fail safe instead of guessing for unsupported or ambiguous cases, including:

- multiple candidate Display Captures without deterministic ownership;
- rotation/skew/flips that are not explicitly proven by the current mapping implementation;
- unsupported bounds modes;
- nested capture ownership that lacks an explicit proven transform chain;
- invalid monitor/source geometry.

Mapping remains read-only with respect to the user's OBS composition.

## Non-negotiable Scene Camera architecture

Do not reintroduce:

- persistent `obs_sceneitem_set_*` transform mutation;
- a custom scene-wide ArZoom Camera input source;
- a private duplicate/off-screen scene render graph solely for zoom/follow;
- a hidden helper scene item for camera motion;
- CPU frame readback;
- a second semantic Smart Camera/planner;
- per-frame file/settings writes;
- unbounded pointer/frame histories.

The accepted Scene Camera remains a managed `arzoom_filter` on the OBS scene source after normal OBS scene composition.

## Regression gates that must stay enabled

The P0–P4 suite remains part of the contract. P4.1 additionally locks tests for:

- generalized read-only mapping and fail-safe ambiguity handling;
- viewport-quality / high-zoom final framing;
- 2× and 4× pointer-loss bounds during real sweeps;
- fixed-target convergence;
- bounded jerk;
- no zero-speed stall while meaningful target travel remains;
- velocity continuity across retarget/handoff;
- at most one intentional velocity-direction reversal for a large target reversal;
- continuous follow pressure;
- bounded tracking-entry count / no chatter;
- no stop-start stalls in real planner traces;
- exact final HOLD with zero center/zoom drift and no target regeneration.

**Do not weaken or delete a regression threshold merely to make a new implementation pass.** If an intentional product change requires a gate change, document the behavioral reason, direct-OBS evidence, before/after trace, and replacement safety/quality contract in the same PR.

## Performance contract

The accepted implementation remains lightweight:

- O(1) bounded camera state;
- no AI/OCR/image analysis;
- no CPU frame readback;
- no growing motion history;
- no per-frame file/settings I/O;
- deterministic platform-neutral camera math;
- GPU-native presentation effects;
- OBS pass-through remains available when presentation effects are inactive.

## Change-control rule

Any future camera-motion change must answer all of these before merge:

1. Does pointer acquisition remain at least as strong at normal and high zoom?
2. Does jerk/velocity continuity remain bounded and visually smooth?
3. Does the camera avoid repeated search/reversal after final pointer intent is known?
4. Does exact HOLD remain drift-free?
5. Are scene mapping and OBS composition still read-only/safe?
6. Do all previous deterministic gates remain green without weakening their intent?
7. Has direct OBS trial confirmed the product character when the motion change is user-visible?

If the answer to any required item is no, the change is not an acceptable successor to the v0.6.0 baseline.
