# P5.3 Safe Presentation-Pass Activation Handshake — Historical / Disproven as Root Cause

**Status:** historical diagnostic experiment. Direct OBS Studio 32.2.2 trial showed no behavioral improvement. Keep this document as failure-analysis history; do not treat the handshake as the confirmed P0 fix.

## Why this existed

Direct OBS trials showed first-click black flicker, persistent black output after Presentation Cursor style changes, and recovery when Toggle Spotlight forced a new processed frame. That initially suggested an unsafe transition from `obs_source_skip_video_filter()` to the first resource-bearing `obs_source_process_filter_begin()/end()` frame.

P5.3 therefore added a bounded neutral warm-up handshake: at most three processed frames after create/update/reactivation/idle→active transitions, with Spotlight disabled, click uniforms cleared, Presentation Cursor hidden, and the permanent transparent sampler bound.

## Direct-OBS result

The direct OBS 32.2.2 trial showed **the same failure pattern with no meaningful improvement**:

- first click still produced a temporary black frame;
- Zoom ON could produce persistent black filtered output;
- Presentation Cursor style changes could produce persistent black output;
- bypassing ArZoom restored Display Capture immediately;
- enabling/toggling Spotlight could restore a valid processed image.

Therefore the warm-frame-only hypothesis is rejected as the root cause.

## Important correction from OBS global data

Official OBS filter documentation and current libobs source establish the intended order:

1. call `obs_source_process_filter_begin()`;
2. set effect parameters;
3. call `obs_source_process_filter_end()` or `obs_source_process_filter_tech_end()`.

`process_filter_begin()` renders/acquires the target into the filter texture when needed. The custom effect is applied at the end stage. This means later P5.4 reasoning that a custom sampler must be bound before `process_filter_begin()` was based on an incorrect model of the OBS API and must not be used as a root-cause claim.

## Current diagnostic direction

The strongest direct behavioral pattern is now:

- pass-through/idle image is normal;
- click-only processed frames become black temporarily;
- Zoom ON processed frames can remain black continuously;
- Presentation Cursor processed frames can remain black continuously;
- Spotlight-active processed frames can render normally.

This shifts investigation away from activation timing and toward **shared processed-effect state / shader isolation**. P5 modified the previously accepted shared `Draw` pixel shader so camera/click/cursor processed frames execute a Spotlight-capable shader even when Spotlight is runtime-off.

The next diagnostic must be evidence-first:

- preserve the accepted v0.6.0 `Draw` technique for non-Spotlight camera/click/cursor frames;
- isolate Spotlight into a distinct technique/path without adding another render pass;
- add transition-only P0 telemetry so OBS logs identify which render route is active when black output occurs;
- perform a controlled A/B against the exact main/v0.6.0 renderer under the same OBS 32.2.2 installation;
- do not add further lifecycle or sampler patches without a log- or source-backed causal finding.

## Performance contract

Any retained diagnostic mechanism must remain bounded. Production P5 must still avoid scene mutation, duplicate scene rendering, CPU frame readback, helper sources, and permanent always-render idle cost.

## Cinematic Spotlight dependency

`P5_CINEMATIC_ZOOM_LINK.md` remains planned and intentionally not wired into runtime until Issue #24 is resolved with direct evidence.
