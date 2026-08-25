# ArZoom Project Direction — Current Source of Truth

**Status:** authoritative for current development direction after v0.5.1.

This document exists to prevent contributors, automation, and AI agents from reviving superseded experiments or following stale roadmap text. When another planning document conflicts with this file, this file and the accepted architecture documents linked below take precedence until an explicit architecture decision changes them.

## Current stable baseline

- Public product line: **ArZoom v0.5.1 for Windows 10/11 x64**.
- Phase 0 through Phase 4 are complete.
- The accepted Smart Camera runtime is shared by the per-source **ArZoom Filter** and the managed scene-level **ArZoom Camera** filter.
- Phase 4 is not an experiment anymore. Its scene-level filter architecture is the baseline for future work.

## Product North Star

ArZoom should become the most pleasant, lightweight, reliable, and beginner-friendly presentation camera for OBS Studio.

The camera should behave like a skilled stabilized camera operator rather than a robotic mouse follower:

1. viewer comfort first;
2. follow presentation areas, not every pointer movement;
3. preserve the accepted Smart Zone → Follow / Catch-Up → Coast → SmoothIdle behavior;
4. keep presentation effects GPU-native and bounded;
5. keep the user's OBS composition safe;
6. keep idle cost very low;
7. make the common path one-click usable;
8. make every development phase independently releasable.

## Accepted Phase 4 architecture

The current scene-wide architecture is:

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
Smart Camera + Presenter Controls + click/cursor effects
       ↓
final scene output
```

ArZoom attaches the existing `arzoom_filter` directly to the current OBS scene source. OBS performs scene composition first; ArZoom processes the completed scene through OBS's normal filter path.

### This is intentional

Future work must preserve these properties unless a new architecture decision is explicitly reviewed and documented:

- no persistent scene-item transform mutation;
- no custom scene-wide `ArZoom Camera` input source;
- no private duplicate scene-render graph;
- no reusable off-screen scene copy solely to implement scene-wide zoom;
- no hidden helper scene item for the camera;
- no CPU frame readback;
- no second Smart Camera/motion engine;
- no per-frame file or settings writes;
- OBS pass-through remains available when presentation effects are inactive.

## Explicitly rejected / superseded directions

The following Phase 4 ideas were explored or proposed earlier and are **not current implementation tasks**:

### Rejected: custom `ArZoom Camera` OBS input source

Do not register a second source type that asks the user to select a scene/source and then re-renders that target into an off-screen texture. That experiment introduced unnecessary source registration, recursion, lifecycle, and duplicate-composition complexity.

### Rejected: Zoominator-style scene-item mutation

Do not implement scene-wide zoom by writing `obs_sceneitem_set_pos`, `obs_sceneitem_set_scale`, rotation, bounds, or crop on the user's composition. ArZoom deliberately avoids the transform-recovery problem by leaving scene items untouched.

### Rejected: parallel motion engine

Do not fork Smart Zone, Coast, SmoothIdle, Presenter Controls, click logic, or Presentation Cursor behavior for Scene Camera. Scene-level and per-source modes share the accepted camera runtime.

## Current limitation to solve

The principal technical gap after Phase 4 is **pointer mapping for complex scene layouts**, not scene rendering.

v0.5.x only enables pointer-driven scene-wide Smart Follow/click/cursor mapping when ArZoom can prove a deterministic mapping for one visible top-level fullscreen Display Capture.

Arbitrary scale, inset placement, crop, rotation, nested presentation layouts, and multiple Display Captures require a more general read-only coordinate mapping system.

## Next engineering priorities

### P4.1 — Generalized read-only scene mapping

Extend Desktop → Display Capture → scene-canvas coordinate mapping without mutating scene items.

Preferred direction:

- inspect source settings and monitor identity;
- read scene-item box/draw transforms;
- compose/invert transforms deterministically;
- support scale/inset and crop first;
- add nested-scene mapping only with an explicit transform chain;
- add rotation only when mathematically proven and covered by tests;
- fail safe with a diagnostic reason when a mapping cannot be proven.

**Invariant:** mapping work is read-only with respect to the user's scene composition.

### P4.2 — Deterministic multi-capture target selection

Multiple Display Captures must not automatically force permanent failure. Add an explicit or otherwise deterministic presentation target selection contract. Never guess which capture owns the pointer.

### Reliability hardening

Before expanding scope aggressively, validate:

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

macOS and Linux remain important for the North Star, but Windows mapping and reliability should be made field-proven first. Platform input backends must feed the same platform-neutral camera model rather than duplicate camera policy.

## Engineering change protocol

Before changing any non-negotiable architecture boundary above:

1. document the problem with a reproducible case;
2. show why the current architecture cannot solve it;
3. compare the proposed design against the existing scene-filter approach;
4. state scene-safety, render-path, lifecycle, performance, and recovery consequences;
5. add/update deterministic regression tests;
6. update this document and the relevant architecture document in the same PR.

A feature request alone is not sufficient reason to revive a rejected architecture.

## Source-of-truth order

For contributors and AI agents, use this order:

1. **`docs/PROJECT_DIRECTION.md`** — current priorities and prohibited/superseded directions.
2. **`docs/PHASE4_ZOOMINATOR_AUDIT.md`** — why the accepted scene-level filter architecture was chosen.
3. **`docs/SMART_CAMERA_ARCHITECTURE.md`** — camera/rendering invariants.
4. **`docs/ARCHITECTURE.md`** — current runtime and coordinate-flow overview.
5. Phase-specific historical specs — useful for accepted behavior, but they do not override the current architecture.
6. GitHub roadmap/issues — planning surfaces; they must be kept synchronized with the documents above.

## Current release boundary

v0.5.1 is a public Windows release that adds product identity/media and packaging hardening on top of the v0.5.0 Scene Camera runtime. It does not replace the accepted Phase 4 architecture.
