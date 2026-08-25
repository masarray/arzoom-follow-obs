# ArZoom Project Direction — Current Source of Truth

**Status:** authoritative for current development direction after v0.6.0.

This document exists to prevent contributors, automation, and AI agents from reviving superseded experiments or regressing the accepted Scene Camera behavior. When another planning document conflicts with this file, this file and the stable-baseline contracts linked below take precedence until an explicit architecture decision changes them.

## Current stable baseline

- Public product line: **ArZoom v0.6.0 for Windows 10/11 x64**.
- Phase 0 through Phase 4.1 are complete.
- Phase 4 scene-wide filter architecture is accepted and must not be reimplemented through scene-item mutation or duplicate scene rendering.
- Phase 4.1 generalized read-only mapping and **Kinematic Smart Viewport** behavior are accepted stable product behavior.
- The direct-OBS accepted P4.1 runtime candidate is recorded in [`P4_1_STABLE_BASELINE.md`](P4_1_STABLE_BASELINE.md).

## Product North Star

ArZoom should become the most pleasant, lightweight, reliable, and beginner-friendly presentation camera for OBS Studio.

The camera should behave like a skilled stabilized camera operator rather than a robotic mouse follower:

1. viewer comfort first;
2. pointer context must stay acquired without continuous cursor chasing;
3. local explanation gestures should remain calm;
4. meaningful relocation should begin promptly and move decisively but smoothly;
5. final framing should be confident and completely still;
6. presentation effects stay GPU-native and bounded;
7. the user's OBS composition remains safe;
8. idle cost remains very low;
9. the common path remains one-click usable;
10. every development phase remains independently releasable.

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
       ↓ normal OBS source-filter pipeline
managed ArZoom Camera filter
       ↓
SceneViewportPlanner (WHERE)
       ↓
SceneKinematicMotion (HOW)
       ↓
Presenter Controls + click/cursor presentation pass
       ↓
final scene output
```

ArZoom attaches the existing `arzoom_filter` directly to the current OBS scene source. OBS composes the scene first; ArZoom processes that completed scene through OBS's normal filter path.

`SceneViewportPlanner` is the semantic camera authority for scene-wide framing. `SceneKinematicMotion` is the accepted deterministic motion synthesizer for reaching the selected target; it does not own scene semantics, pointer intent policy, or OBS composition.

## Non-negotiable architecture boundaries

Future work must preserve these properties unless a new architecture decision is explicitly reviewed and documented:

- no persistent scene-item transform mutation;
- no custom scene-wide `ArZoom Camera` input source;
- no private duplicate scene-render graph;
- no reusable off-screen scene copy solely for scene-wide zoom;
- no hidden helper scene item for camera motion;
- no CPU frame readback;
- no second semantic Smart Camera/planner competing with `SceneViewportPlanner`;
- no per-frame file or settings writes;
- no unbounded pointer/frame histories;
- OBS pass-through remains available when presentation effects are inactive.

## Explicitly rejected / superseded directions

### Rejected: custom ArZoom Camera OBS input source

Do not register a second source type that asks the user to select a scene/source and then re-renders that target into an off-screen texture. That experiment introduced unnecessary source registration, recursion, lifecycle, and duplicate-composition complexity.

### Rejected: Zoominator-style scene-item mutation

Do not implement scene-wide zoom by writing `obs_sceneitem_set_pos`, `obs_sceneitem_set_scale`, rotation, bounds, or crop on the user's composition. ArZoom deliberately avoids the transform-recovery problem by leaving scene items untouched.

### Rejected: competing camera/motion policy

Do not create a second semantic follow engine, parallel Smart Zone policy, or another planner that competes with the accepted Scene Camera authority. Improvements to physical motion belong in the shared deterministic kinematic layer unless an explicit architecture decision proves otherwise.

## P4.1 stable behavior — locked

The complete behavioral contract lives in [`P4_1_STABLE_BASELINE.md`](P4_1_STABLE_BASELINE.md). Key requirements are:

- local pointer work remains calm;
- leaving useful viewport context begins tracking promptly;
- normal/high-zoom far movement keeps pointer acquisition bounded;
- far travel may cruise faster but remains acceleration/jerk smooth;
- no stop → restart → stop cadence;
- no repeated searching/reversal after final pointer intent is known;
- final pointer lands in a useful near-centre contextual area;
- final HOLD is exact and drift-free;
- TRACK → final SETTLE preserves meaningful velocity/acceleration continuity;
- pressure changes HOW decisively the camera moves, not WHERE the target corridor continuously shifts;
- regression gates must not be weakened merely to make a new implementation pass.

## P4.1 read-only mapping boundary

v0.6.0 supports deterministic mapping for exactly one visible top-level Display Capture when the mapping can be proven, including:

- positive axis-aligned scale/inset placement;
- crop-aware mapping;
- deterministic monitor ownership;
- one shared mapped path for Smart Follow, click anchoring, and Presentation Cursor;
- Presentation Cursor size tied to exact live camera zoom.

ArZoom fails safe instead of guessing for unsupported/ambiguous cases such as multiple candidate Display Captures, unproven rotation/skew/flips, unsupported bounds modes, unresolved nested capture ownership, or invalid monitor/source geometry.

**Invariant:** mapping remains read-only with respect to the user's scene composition.

## Next engineering priorities

### P4.2 — Deterministic multi-capture target selection

Multiple Display Captures must not force permanent failure, but ArZoom must never guess capture ownership. Add explicit or otherwise deterministic presentation-target selection while preserving the v0.6.0 mapping and motion contracts.

### Reliability hardening

Continue field/soak validation for:

- scene changes while zoomed;
- rapid enable/disable and hotkey stress;
- OBS restart and shutdown;
- monitor disconnect/reconnect;
- Windows sleep/resume;
- 100/125/150/175/200% mixed DPI;
- negative virtual-desktop coordinates;
- Intel / AMD / NVIDIA GPUs;
- 30 / 60 / 120 / 144 fps;
- standard and portable OBS installs;
- OBS current and next-major compatibility.

### UX simplification

The common workflow should approach:

```text
Install → OBS Tools → Enable ArZoom Scene Camera → present
```

Advanced camera, cursor, mapping, and diagnostics controls should remain available without burdening first-time users.

### Cross-platform later

macOS and Linux remain important for the North Star, but Windows reliability should be made field-proven first. Platform input backends must feed the same platform-neutral camera model rather than duplicate camera policy.

## Engineering change protocol

Before changing any non-negotiable architecture or P4.1 motion-quality boundary:

1. document the reproducible problem;
2. show why the current architecture/behavior cannot solve it;
3. state scene-safety, render-path, lifecycle, performance, and recovery consequences;
4. preserve or strengthen pointer-acquisition and motion-quality gates together;
5. provide before/after deterministic traces for user-visible motion changes;
6. run direct OBS trial for user-visible motion changes;
7. update this document and the relevant stable-baseline/architecture document in the same PR.

A feature request alone is not sufficient reason to revive a rejected architecture or weaken an accepted regression gate.

## Source-of-truth order

For contributors and AI agents, use this order:

1. **`docs/PROJECT_DIRECTION.md`** — current priorities and prohibited/superseded directions.
2. **`docs/P4_1_STABLE_BASELINE.md`** — accepted v0.6.0 mapping and Kinematic Smart Viewport regression contract.
3. **`docs/PHASE4_ZOOMINATOR_AUDIT.md`** — why the accepted scene-level filter architecture was chosen.
4. **`docs/SMART_CAMERA_ARCHITECTURE.md`** — camera/rendering invariants.
5. **`docs/ARCHITECTURE.md`** — runtime and coordinate-flow overview.
6. Phase-specific historical specs — useful for accepted behavior, but they do not override current contracts.
7. GitHub roadmap/issues — planning surfaces; they must stay synchronized with the documents above.

## Current release boundary

v0.6.0 is the first stable public release that combines generalized P4.1 read-only scale/inset/crop mapping with the accepted Kinematic Smart Viewport behavior. The release does not change the accepted Phase 4 scene-level filter architecture; it strengthens mapping coverage and the physical quality/reliability of pointer-aware viewport movement.
