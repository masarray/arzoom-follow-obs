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
- Active implementation blueprint for the next tutorial-camera capabilities:
  - [`TUTORIAL_CAMERA_IMPLEMENTATION_BLUEPRINT.md`](TUTORIAL_CAMERA_IMPLEMENTATION_BLUEPRINT.md)
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
12. hard runtime bugs require evidence-first diagnosis and deterministic regression gates;
13. optimize for real tutorial readability: make the work area large and clear while allowing broadcast overlays such as facecam/logo to remain visually stable when desired.

## Accepted Scene Camera architecture

```text
OBS Scene / Presentation Layer
├─ Display Capture(s)
├─ Browser / tutorial content
└─ other zoomable content
       ↓ OBS native composition
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
final scene output / optional parent composition
       ├─ fixed Facecam
       ├─ fixed Logo
       └─ fixed Lower Third / status overlay
```

`SceneViewportPlanner` is the semantic camera authority. `SceneKinematicMotion` is the accepted physical motion synthesizer. Spotlight is presentation-only and may read mapped input, camera transform, bounded presentation events, and accepted focus context; it may not write camera intent.

An explicit user-owned nested scene may act as a **Presentation Layer** beneath fixed parent overlays. That remains normal OBS-native composition and is different from a hidden helper source or private duplicate scene render.

## Non-negotiable architecture boundaries

Unless a separately reviewed architecture decision supersedes them:

- no persistent per-frame scene-item transform mutation;
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

Explicit, visible and user-owned OBS nested scenes remain valid architecture. Any future automatic scene setup/restructuring must be user-approved, reversible, transaction-safe, and must not create hidden composition state.

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

The implementation sequencing and per-slice gates live in [`TUTORIAL_CAMERA_IMPLEMENTATION_BLUEPRINT.md`](TUTORIAL_CAMERA_IMPLEMENTATION_BLUEPRINT.md).

### P4.2 — Multi-Screen Smart Camera — #25

Primary user outcome: a creator can build one tutorial scene containing multiple captured presentation displays — for example **Coding/IDE on Monitor 1** and **Application/browser/HMI on Monitor 2** — and ArZoom makes the screen currently being used clearly readable to viewers without manual camera operation.

Required direction:

- keep exactly one scene-level ArZoom Camera;
- Display Captures are coordinate references, not independently zoomed sources;
- allow multiple user-eligible **Presentation Screens**;
- deterministically resolve physical monitor → Display Capture → scene mapping;
- select the active screen from the actual physical monitor containing the cursor;
- utility/OBS/chat monitors may remain excluded;
- install exactly one active scene-mapped coordinate owner at a time;
- preserve the existing SceneViewportPlanner and SceneKinematicMotion as the only camera authority;
- feed the same active mapping to Smart Follow, click, Presentation Cursor, and Spotlight;
- moving A → B changes presentation context but must not reset camera kinematics or replay Cinematic Spotlight;
- ambiguity or invalid geometry fails safe instead of choosing the nearest/largest capture heuristically;
- direct dual-screen coding/application acceptance is required before stable promotion.

Engineering issue: [#25 — P4.2: Multi-Screen Smart Camera for dual/multi-display tutorials](../issues/25)

### P4.3 — Protected Overlays / zoomable Presentation Layer — #26

Primary user outcome: code/application content can zoom enough to become readable while selected broadcast overlays such as **facecam, logo, lower third, timer/status widgets, or fixed browser overlays** remain the same size and position in the viewer frame.

This is a composition-ownership problem. The current post-composition Scene Camera cannot simply recover individual facecam/logo pixels from the already-flattened final image.

Preferred first architecture to prove:

```text
Main Tutorial Scene
├─ Scene: Presentation Content   ← ArZoom Camera here
│    ├─ Coding Display Capture
│    ├─ Application Display Capture
│    └─ zoomable tutorial content
├─ Facecam                       ← fixed overlay
├─ Logo                          ← fixed overlay
└─ Lower Third / status UI       ← fixed overlay
```

Required direction:

- prove the manual OBS-native nested-scene workflow before automating setup;
- #25 Multi-Screen mapping operates inside the Presentation Content coordinate space;
- keep facecam/logo/overlays outside the camera transform in the parent scene;
- do not fake source exclusion with runtime scene-item transform mutation;
- no hidden helper scenes/sources or private full-scene duplicate rendering;
- any later setup assistant must be explicit, previewable, reversible and user-approved;
- direct tutorial-layout acceptance is required before stable promotion.

Engineering issue: [#26 — P4.3: Protected Overlays — keep facecam/logo/web fixed while presentation content zooms](../issues/26)

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

Setup Doctor should explain failures in user language such as missing/ambiguous Presentation Screens or invalid layout geometry rather than exposing internal mapper terminology.

### Complex composition mapping — later

After #25/#26 and reliability are proven:

- nested transform-chain mapping where deterministic math can prove it;
- rotation support only with deterministic math/tests;
- broader crop/bounds/layout support;
- source identity/lifecycle hardening.

### UX refinement

Keep the common flow close to:

```text
Install → OBS Tools → Enable Scene Camera → Configure → present
```

For multi-screen/tutorial composition, prefer user-language concepts such as **Presentation Screens**, **Presentation Content**, and **Keep fixed** over internal concepts such as mapper ownership or scene-item transforms.

Use progressive disclosure. Beginner controls describe outcomes; advanced geometry, diagnostics, custom cursor assets, and tuning stay secondary.

### Cross-platform later

macOS and Linux remain North Star goals after Windows reliability is field-proven. Platform input backends must feed the same camera/presentation model rather than fork policy.

## Engineering change protocol

For camera, mapping, renderer, or shared shader changes:

1. reproduce the problem or define the user-visible capability precisely;
2. identify the smallest existing seam that owns the behavior;
3. preserve accepted invariants before adding capability;
4. prefer pure deterministic model/tests before OBS/runtime wiring;
5. implement one reversible slice at a time;
6. run the slice-specific gate plus all existing regression gates;
7. if a gate fails, stop and diagnose before stacking another fix;
8. run Windows compile/package CI;
9. directly trial user-visible behavior in OBS;
10. update the stable contract only in the accepted release line.

A passing compile is not direct-OBS acceptance, and a visual workaround is not a substitute for causal diagnosis.

For #25/#26 specifically, follow the slice order, stop conditions, PR discipline and handoff protocol in [`TUTORIAL_CAMERA_IMPLEMENTATION_BLUEPRINT.md`](TUTORIAL_CAMERA_IMPLEMENTATION_BLUEPRINT.md).

## Source-of-truth order

1. `docs/PROJECT_DIRECTION.md`
2. `docs/P4_1_STABLE_BASELINE.md`
3. `docs/P5_STABLE_BASELINE.md`
4. `docs/TUTORIAL_CAMERA_IMPLEMENTATION_BLUEPRINT.md` for active #25/#26 implementation strategy
5. accepted architecture decision documents
6. historical phase/P5 design and recovery documents
7. GitHub roadmap/issues, which must remain synchronized with the documents above.

Stable baselines always override future-feature planning when a proposed implementation would regress shipped behavior.

## Current release boundary

**v0.7.0** ships the accepted v0.6.0 camera/mapping baseline plus P5 Spotlight, Cinematic Spotlight with Zoom, D3D11-safe shared shader initialization, zero-refresh Spotlight runtime controls, and resize-only Spotlight behavior during Zoom +/-.

P4.2 Multi-Screen Smart Camera (#25) and P4.3 Protected Overlays (#26) are **not** included in v0.7.0. They are active future work and must remain isolated from `main` runtime until their deterministic gates and direct OBS acceptance pass.
