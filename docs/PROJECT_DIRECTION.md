# ArZoom Project Direction — Current Source of Truth

**Status:** authoritative after ArZoom v0.7.0.

This document prevents contributors, automation, and AI agents from reviving superseded experiments or regressing accepted public behavior. When historical planning documents conflict with this file or the stable-baseline documents linked below, the current stable baselines take precedence.

## Current stable product line

- Public stable: **ArZoom v0.7.0 for Windows 10/11 x64**.
- Phase 0 through P5 Spotlight are shipped.
- P4.1 generalized read-only mapping + Kinematic Smart Viewport remains the accepted camera/motion baseline.
- P5 Spotlight + Cinematic Zoom focus is the accepted presentation-attention baseline.
- Stable contracts:
  - [`P4_1_STABLE_BASELINE.md`](P4_1_STABLE_BASELINE.md)
  - [`P5_STABLE_BASELINE.md`](P5_STABLE_BASELINE.md)
- Pre-release P5 design/hotfix documents remain useful engineering history but do not override the v0.7.0 stable baseline.

## Product North Star

ArZoom should become the most pleasant, lightweight, reliable, and beginner-friendly presentation camera for OBS Studio.

Principles:

1. viewer comfort first;
2. follow presentation context, not every pointer sample;
3. keep meaningful pointer context acquired;
4. move decisively but smoothly;
5. settle exactly and remain still;
6. keep presentation effects GPU-native and bounded;
7. preserve the user's OBS composition;
8. keep idle cost very low;
9. keep common setup beginner-friendly;
10. make every phase independently releasable;
11. attention effects consume camera/mapping intent but never become camera authority;
12. hard runtime bugs require evidence-first diagnosis and deterministic regression gates.

## Accepted Scene Camera architecture

```text
OBS Scene
├─ Display Capture
├─ Webcam
├─ Browser
├─ Logo
└─ Nested Scene
       ↓ OBS native scene composition
scene obs_source_t
       ↓ normal OBS filter pipeline
managed ArZoom Camera filter
       ↓
read-only mapping
       ↓
SceneViewportPlanner — WHERE
       ↓
SceneKinematicMotion — HOW
       ↓
Presenter Controls + GPU presentation effects
       ├─ click feedback
       ├─ Presentation Cursor
       └─ Spotlight / Cinematic Zoom focus
       ↓
final scene output
```

`SceneViewportPlanner` is the semantic camera authority. `SceneKinematicMotion` is the accepted physical motion synthesizer. Spotlight is presentation-only and may read mapped input, camera transform, bounded presentation events, and accepted focus context; it may not write camera intent.

## Non-negotiable architecture boundaries

Unless a separately reviewed architecture decision supersedes them:

- no persistent scene-item transform mutation;
- no custom scene-wide ArZoom Camera input source;
- no duplicate/off-screen scene-render graph;
- no hidden helper scene item/source for camera or Spotlight;
- no CPU frame readback;
- no second semantic Smart Camera/planner;
- no presentation effect that wakes, retargets, or accelerates camera intent;
- no per-frame file/settings writes;
- no unbounded pointer/frame/click histories;
- no generated PNG/browser overlay production path;
- no default multi-pass blur/bloom/particle pipeline;
- OBS pass-through remains available when presentation effects are inactive.

## P4.1 stable behavior — locked

The complete contract lives in [`P4_1_STABLE_BASELINE.md`](P4_1_STABLE_BASELINE.md).

Key invariants:

- local pointer work remains calm;
- meaningful relocation begins promptly;
- normal/high-zoom pointer loss stays bounded;
- far travel may cruise faster while acceleration/jerk remain smooth;
- no stop → restart → stop cadence;
- no repeated searching after final pointer intent is known;
- final framing is useful and exact;
- TRACK → SETTLE preserves meaningful velocity/acceleration continuity;
- final HOLD is drift-free;
- mapping remains read-only and fails safe on ambiguity.

## P5 Spotlight stable behavior — locked

The complete contract lives in [`P5_STABLE_BASELINE.md`](P5_STABLE_BASELINE.md).

Accepted v0.7.0 product behavior:

- exactly three behavior modes: Smart Focus, Follow cursor, Click to lock;
- default mode: Follow cursor;
- default shape: Circle;
- default area: 170%;
- default background dim: 35%;
- default edge softness: 40 px;
- default Presentation Cursor: ArZoom Classic Hand;
- Spotlight master controls remain Off by default;
- Cinematic Spotlight with Zoom is enabled by default once Spotlight controls are enabled;
- Toggle Zoom ON/OFF owns cinematic close/open;
- Zoom +/- is resize-only and never replays cinematic focus;
- manual Toggle/Hold/Peek remain independent presenter intent;
- shared D3D11 Draw ABI is fully initialized on every processed frame;
- runtime Toggle/Peek actions never rebuild the OBS Properties sheet.

## Permanent P5 reliability lessons

Direct OBS development exposed two real classes of regression that are now permanent gates:

1. **Shader ABI completeness:** every uniform declared by the shared Draw technique must receive a deterministic value before processed rendering. Optional effects must use neutral values when inactive.
2. **Properties zero-refresh runtime actions:** presenter buttons/hotkeys that only change runtime state must not request an OBS Properties rebuild.

Speculative fixes that were disproven by direct evidence must not be revived as accepted root causes.

## Next engineering priorities

### P4.2 — deterministic multi-capture target selection

Multiple Display Captures should not remain a blanket failure mode, but ArZoom must never guess ownership.

Required direction:

- explicit or otherwise deterministic presentation-target ownership;
- preserve the v0.7.0 P4.1 motion and mapping contracts;
- shared selection for Smart Follow, click, cursor, and Spotlight;
- diagnostics explain why ownership is or is not proven;
- ambiguous ownership disables pointer-driven behavior instead of selecting heuristically.

### Reliability / Setup Doctor

Continue field/soak hardening for:

- scene changes while zoomed;
- rapid enable/disable/hotkey/click/Spotlight stress;
- monitor disconnect/reconnect;
- sleep/resume;
- mixed DPI and negative desktop coordinates;
- Intel / AMD / NVIDIA;
- 30 / 60 / 120 / 144 fps;
- standard and portable OBS;
- current and next-major OBS compatibility;
- compact diagnostics that make failures actionable.

### UX refinement

Keep the common flow close to:

```text
Install → OBS Tools → Enable Scene Camera → Configure → present
```

Use progressive disclosure. Beginner controls describe outcomes; advanced geometry, diagnostics, custom cursor assets, and tuning stay secondary.

### Cross-platform later

macOS and Linux remain North Star goals after Windows reliability is field-proven. Platform input backends must feed the same camera/presentation model rather than fork policy.

## Engineering change protocol

For camera, mapping, renderer, or shared shader changes:

1. reproduce the problem;
2. minimize it;
3. gather evidence before patching;
4. rank hypotheses;
5. instrument when the cause is not observable;
6. fix the causal defect;
7. add a deterministic regression gate;
8. run Windows compile/package CI;
9. directly trial user-visible behavior in OBS;
10. update the stable contract in the same release line.

A passing compile is not direct-OBS acceptance, and a visual workaround is not a substitute for causal diagnosis.

## Source-of-truth order

1. `docs/PROJECT_DIRECTION.md`
2. `docs/P4_1_STABLE_BASELINE.md`
3. `docs/P5_STABLE_BASELINE.md`
4. architecture decision documents
5. historical phase/P5 design and recovery documents
6. GitHub roadmap/issues, which must remain synchronized with the documents above.

## Current release boundary

**v0.7.0** ships the accepted v0.6.0 camera/mapping baseline plus P5 Spotlight, Cinematic Spotlight with Zoom, D3D11-safe shared shader initialization, zero-refresh Spotlight runtime controls, and resize-only Spotlight behavior during Zoom +/-.

P4.2 deterministic multi-capture target selection is not included and remains future work. ArZoom continues to fail safe rather than guess presentation capture ownership.
