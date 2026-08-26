# P5 Spotlight Stable Baseline — ArZoom v0.7.0

**Status:** accepted public stable contract for ArZoom v0.7.0.

This document locks the P5 Spotlight behavior that passed deterministic CI and direct OBS acceptance. Future work may extend P5, but must preserve these user-visible and architectural invariants unless a new explicit acceptance supersedes them.

## Release acceptance

Accepted direct OBS behavior:

- fresh ArZoom filter does not flicker or blank the preview on click;
- Toggle Zoom ON/OFF renders normally;
- Presentation Cursor style changes do not leave the preview black;
- disabling/bypassing ArZoom is not required to recover video;
- Spotlight Toggle is not a render-recovery mechanism;
- Spotlight GUI Toggle does not rebuild/flicker the OBS Properties sheet;
- Cinematic Spotlight with Zoom closes/opens smoothly and was accepted visually;
- Zoom +/- does not replay cinematic focus and is resize-only;
- final v24 candidate passed 19/19 deterministic tests plus Windows C++/shader compile and installer packaging.

## Product defaults

The stable v0.7.0 Spotlight defaults are:

- master **Enable Spotlight controls**: Off;
- Spotlight mode: **Follow cursor**;
- focus shape: **Circle**;
- Spotlight Area Size: **170%**;
- Background Dim: **35%**;
- Edge Softness: **40 px**;
- Presentation Cursor: **ArZoom Classic Hand**;
- Cinematic Spotlight with Zoom: On once Spotlight controls are enabled;
- Cinematic Focus Speed: **Balanced**.

The master remains Off so adding an ArZoom filter never places an attention effect on-air without presenter intent.

## Exactly three behavior modes

1. **Smart Focus** — read-only consumer of accepted camera/context information. It cannot write camera intent.
2. **Follow cursor** — follows the proven mapped pointer with bounded visual-only smoothing. This is the stable default.
3. **Click to lock** — stores one content-space click anchor and keeps it attached to content through camera movement.

Shape, dim, size, softness, and cinematic timing are appearance/choreography controls, not additional behavior modes.

## Cinematic Spotlight with Zoom

Toggle Zoom ON/OFF may drive a presentation-only iris animation:

```text
full bright frame
    ↓ Zoom ON
full-frame aperture
    ↓ minimum-jerk close
configured focus Circle + configured dim
    ↓ Zoom OFF
minimum-jerk open
    ↓
full bright frame / Spotlight runtime inactive
```

Rules:

- focus target is acquired while the aperture is still visually full-frame;
- the full aperture covers the farthest canvas corner from the actual focus center;
- dimming trails the initial aperture motion to avoid a dark pop;
- mid-animation Zoom reversal continues from the current visual state;
- animation is time-based and frame-rate independent;
- no camera target, velocity, acceleration, safe-zone pressure, or scene item may be modified by Spotlight choreography.

Stable engineering timing presets:

| Preset | Close | Open |
| --- | ---: | ---: |
| Smooth | 480 ms | 400 ms |
| Balanced | 360 ms | 300 ms |
| Snappy | 260 ms | 220 ms |

These timings are implementation constants of v0.7.0, not a promise that future releases cannot retune them after direct acceptance.

## Zoom +/- contract

A critical v24 behavior lock:

```text
Toggle Zoom ON/OFF → cinematic close/open
Zoom In / Zoom Out → resize-only
```

While zoom is active, Increase/Decrease must not reset or replay the cinematic iris. Camera magnification, Presentation Cursor size, and Spotlight aperture resize smoothly from the live camera zoom state.

Rapid +/- retargeting remains continuous and bounded. Toggle Zoom OFF afterwards still performs the normal cinematic opening.

## Renderer ownership

P5 must not own a duplicate camera/click/cursor renderer.

Accepted architecture:

```text
camera / click / Presentation Cursor / Spotlight
                ↓
       one shared presentation pass
```

Forbidden:

- duplicate scene render;
- helper OBS source or scene item;
- CPU frame readback;
- CPU-rasterized Spotlight mask;
- generated PNG/browser overlay production effect;
- default blur/bloom/particle pass;
- presentation code that feeds authority back into camera planning.

## D3D11 shader ABI invariant

This is a permanent regression gate established by the direct OBS black-screen investigation.

**Every parameter declared by the shared Draw technique must receive a deterministic value before a processed draw.**

Optional feature parameters must be explicitly set to neutral values when inactive. Never rely on a branch inside the shader to excuse an unset effect parameter.

The P5 failure signature observed during development was:

```text
device_draw (D3D11): Not all shader parameters were set
```

The accepted implementation primes the complete neutral Spotlight ABI before optional active values overwrite it. Cinematic extension uniforms are likewise initialized to neutral values on every processed Draw.

## GUI runtime-action invariant

Runtime presenter actions must not rebuild the OBS Properties sheet.

- Toggle Spotlight: zero-refresh runtime action;
- Spotlight Peek: zero-refresh runtime action;
- hotkeys: runtime state only.

The former button callback returned `true`, forcing a property-sheet rebuild that appeared as rapid slider/widget flicker. This is fixed and regression-locked.

## Performance contract

Spotlight remains lightweight:

- analytic GPU mask in the shared presentation pass;
- O(1) bounded state;
- no growing pointer/click/animation history;
- no CPU frame analysis;
- no OCR/AI;
- no per-frame file/settings writes;
- no new texture pass for cinematic choreography;
- normal pass-through remains available when no presentation effect requires processing.

## Mapping and focus ownership

Spotlight consumes the same proven read-only mapping used by Smart Follow, click anchoring, and Presentation Cursor. It does not add a second Display Capture resolver.

Ambiguous/unproven mapping must fail safe rather than guess.

## Deterministic gates

The v0.7.0 release line preserves P0–P4.1 tests and adds/retains P5 gates for:

- Spotlight activation and geometry;
- true Circle sizing;
- neutral shader ABI completeness;
- cinematic minimum-jerk endpoints/monotonicity;
- full-frame radius coverage including edge/corner focus;
- delayed dim choreography;
- mid-animation reversal continuity;
- frame-rate independence;
- Zoom +/- resize-only behavior;
- cursor sampler/render-safety historical regression coverage.

Final accepted v24 candidate: **19/19 deterministic tests passed** before Windows packaging.

## Historical investigation notes

The P5_1/P5_2/P5_3/P5_4 and cinematic design documents remain in the repository as engineering history. Some contain trial hypotheses that were later disproven. They do not override this stable baseline.

In particular, the accepted black-screen root cause was the incomplete shared shader ABI, not the earlier warm-frame or sampler-order hypotheses.

## Future changes

Future P5 work must preserve:

- zero black-frame regression;
- complete shader ABI initialization;
- zero-refresh runtime UI actions;
- single renderer ownership;
- camera isolation;
- Toggle Zoom cinematic semantics;
- Zoom +/- resize-only semantics;
- low idle cost.

Any change to these behaviors requires deterministic tests and direct OBS acceptance before stable promotion.
