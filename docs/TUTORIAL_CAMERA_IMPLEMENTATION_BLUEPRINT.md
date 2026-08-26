# Tutorial Camera Implementation Blueprint — P4.2 Multi-Screen + P4.3 Protected Overlays

**Status:** accepted implementation blueprint / handoff source of truth  
**Parent roadmap:** #3  
**Primary engineering issues:** #25, #26  
**Stable product baseline:** ArZoom v0.7.0  
**Do not treat this document as permission to weaken:** `P4_1_STABLE_BASELINE.md`, `P5_STABLE_BASELINE.md`

---

## 1. Product outcome first

ArZoom should make real tutorial work readable to viewers without forcing the presenter to manually operate a virtual camera.

### P4.2 — Multi-Screen Smart Camera

A presenter can compose multiple captured displays in one tutorial scene:

```text
OBS canvas
┌──────────────────────┬──────────────────────┐
│ Monitor 1            │ Monitor 2            │
│ Coding / IDE         │ App / browser / HMI  │
└──────────────────────┴──────────────────────┘
```

When the cursor is on Monitor 1, the **one scene-level ArZoom Camera** focuses the corresponding coding region of the whole scene. When the cursor crosses to Monitor 2, the same camera travels smoothly to the application region. The audience sees a readable camera shot rather than two permanently tiny half-screen captures.

### P4.3 — Protected Overlays

The presentation content may zoom aggressively while broadcast overlays can remain visually fixed:

```text
Main Tutorial Scene
├─ Presentation Content scene  ← camera may zoom/pan
│   ├─ Coding Display Capture
│   ├─ Application Display Capture
│   └─ zoomable tutorial content
├─ Facecam                     ← fixed
├─ Logo                        ← fixed
└─ Lower Third / status UI     ← fixed
```

The user benefit is not an `exclude` checkbox by itself. The benefit is **large readable code/application content without enlarging or throwing the facecam/logo off-screen**.

---

## 2. Engineering operating mode

Use a disciplined small-slice implementation model.

1. **Preserve accepted invariants before adding capability.**
2. **Extract or add one seam at a time.** Do not combine a structural refactor, UI redesign and behavior change in one commit.
3. **Prefer pure deterministic models first**, then wire OBS runtime APIs around them.
4. **One owner per piece of state.** Do not let camera, click, cursor and Spotlight choose different presentation screens.
5. **No speculative renderer changes.** Rendering is not the P4.2 problem.
6. **Every slice must be reversible** and have an explicit regression gate.
7. **Do not continue after a failed gate by stacking another fix.** Diagnose the failing invariant first.
8. **Direct OBS acceptance is a separate gate from CI.** Green deterministic tests are necessary, not sufficient.
9. **Do not promote documentation/marketing claims until direct acceptance passes.**
10. **Keep `main` releasable.** Development lives on an isolated feature branch / Draft PR.

Recommended branch for #25:

```text
feature/p4.2-multi-screen-camera
```

Recommended later branch for #26:

```text
feature/p4.3-protected-overlays
```

Do not implement both phases in one PR.

---

## 3. Regression-locked architecture

The existing accepted architecture remains:

```text
OBS native scene composition
          ↓
managed ArZoom Camera filter
          ↓
shared read-only pointer mapping
          ↓
SceneViewportPlanner — WHERE
          ↓
SceneKinematicMotion — HOW
          ↓
click / Presentation Cursor / Spotlight
          ↓
shared presentation renderer
```

Hard prohibitions unless a separately reviewed architecture decision supersedes them:

- no persistent per-frame `obs_sceneitem_set_*` transform mutation;
- no private duplicate render of the complete scene;
- no hidden helper source/scene item;
- no CPU frame readback;
- no second Smart Camera/planner;
- no per-frame settings/file writes;
- no unbounded motion/click/focus history;
- no renderer rewrite to solve coordinate ownership;
- no reintroduction of the P5 black-frame regressions.

P5 renderer/ABI contract remains regression-locked:

- every shared Draw shader parameter receives a deterministic value;
- Toggle Zoom ON/OFF owns cinematic close/open;
- Zoom +/- remains resize-only;
- runtime Spotlight controls do not rebuild/flicker OBS Properties.

---

## 4. Current code seam — what exists today

### 4.1 Mapping math is already reusable

`src/arzoom-scene-camera-core.hpp` contains the platform-neutral mapping primitives:

- `SceneMappingQuad`
- `SceneAxisAlignedMapping`
- `scene_mapping_build_axis_aligned(...)`
- `scene_mapping_source_to_scene(...)`
- source-visible and scene-visible gates

These already support the accepted P4.1 fullscreen / scale / inset / proven crop contract.

**Do not replace this math for P4.2.** Each Presentation Screen should simply own one independently proven `SceneAxisAlignedMapping`.

### 4.2 The current limitation is concentrated in P4.1 discovery

`src/arzoom-filter-v12.cpp` currently contains:

```text
find_single_scene_display_capture(...)
        ↓
resolve_phase41_layout(...)
        ↓
SceneAxisAlignedMapping
        ↓
build_mapped_monitor(...)
        ↓
phase1->monitor = synthetic mapped MonitorDescriptor
```

`find_single_scene_display_capture(...)` deliberately fails when more than one visible top-level Display Capture exists.

This is the exact P4.2 replacement seam.

### 4.3 Existing consumers already share one mapping

P4.1 installs one synthetic `MonitorDescriptor` into the inherited P1 runtime. Existing Smart Camera, click capture, Presentation Cursor and Spotlight all consume that shared state.

Therefore P4.2 should preserve this invariant:

> **At any instant there is exactly one active synthetic scene-mapped monitor descriptor.**

Multi-screen support means selecting which proven mapping supplies that descriptor. It does **not** mean making each downstream feature multi-screen aware.

This is the lowest-risk path because it preserves the already accepted downstream normalization behavior.

### 4.4 Current wrapper head

The public v0.7.0 runtime compiles `src/arzoom-filter-v24.cpp`, which wraps the accepted P5 cinematic + resize-only behavior.

Do not start #25 by adding a large `v25` wrapper that duplicates P4.1 internals. First create/extract the mapping resolver seam described below; only then wire the smallest final runtime override necessary.

---

## 5. OBS identity contract — verified baseline

OBS Studio 31.1.1 exposes:

```cpp
const char *obs_source_get_uuid(const obs_source_t *source);
obs_source_t *obs_get_source_by_uuid(const char *uuid);
```

Use **source UUID** as the durable persisted identity for Presentation Screens.

Rules:

- source name is display text only;
- scene-item order/index is never durable identity;
- source pointer is runtime identity only, never persisted;
- a renamed source should remain selected when its UUID remains the same;
- a deleted/recreated source with a new UUID is a different source and must not silently inherit ownership;
- UUID resolution failure is diagnostic/fail-safe, not a fallback-to-similar-name heuristic.

---

## 6. P4.2 target architecture

```text
                         Windows cursor
                               ↓
                   physical monitor containment
                               ↓
                PresentationScreenResolver
                               ↓
             one ActivePresentationMapping
                               ↓
        existing synthetic MonitorDescriptor adapter
                               ↓
                    phase1->monitor
                               ↓
      ┌────────────────────────┼────────────────────────┐
      ↓                        ↓                        ↓
SceneViewportPlanner       click/cursor              Spotlight
      ↓
SceneKinematicMotion
      ↓
ONE scene-level ArZoom Camera
```

### Required pure model

Add a small platform-neutral file, recommended:

```text
src/arzoom-presentation-screen-resolver.hpp
```

Suggested conceptual data model:

```cpp
struct PresentationScreenCandidate {
    std::string source_uuid;
    MonitorRect physical_monitor;
    SceneAxisAlignedMapping mapping;
    bool eligible;
    bool visible;
    bool monitor_resolved;
    bool geometry_valid;
};

enum class PresentationScreenResolveStatus {
    Active,
    NoEligibleScreens,
    CursorOutsideEligibleScreens,
    AmbiguousMonitorOwnership,
    ActiveScreenInvalid,
};

struct PresentationScreenResolveResult {
    PresentationScreenResolveStatus status;
    size_t active_index;
};
```

The actual names may change, but the ownership model must not.

The pure resolver receives already-discovered candidates + cursor desktop coordinates. It must not call OBS or Win32 APIs.

### Selection rule

A candidate may become active only if:

1. it is user-eligible;
2. its source UUID resolves to the expected Display Capture;
3. its physical monitor is deterministically resolved;
4. its mapping geometry is valid under the P4.1 contract;
5. the cursor lies inside that physical monitor rectangle.

If two eligible candidates claim the same physical monitor containing the cursor, result = ambiguous/fail-safe.

If the cursor is on a utility/non-selected display, keep camera state stable and do not guess a nearest screen.

---

## 7. P4.2 implementation slices

Each slice should be one logical commit or a very small cluster with one purpose.

### Slice 0 — Blueprint + baseline freeze

**Goal:** prevent architecture drift before coding.

Gate:

- v0.7.0 remains current stable;
- 19/19 current deterministic tests remain the regression baseline;
- #25 points to this blueprint;
- no runtime changes.

### Slice 1 — Pure Multi-Screen resolver

Add:

```text
src/arzoom-presentation-screen-resolver.hpp
tests/arzoom-phase42-presentation-screen-resolver-test.cpp
```

Test before OBS wiring:

1. zero candidates;
2. one eligible candidate, cursor inside;
3. two candidates on different monitors, cursor A;
4. cursor B;
5. cursor outside both;
6. duplicate candidates claiming same monitor;
7. invalid/hidden candidate under cursor;
8. negative desktop coordinates;
9. disabled utility screen never activates;
10. rapid A/B input sequence has deterministic results and bounded state.

**Gate:** new pure test green + all existing tests green.

No OBS Properties and no renderer work in this slice.

### Slice 2 — Behavior-neutral mapping seam extraction

The current single-capture discovery in `arzoom-filter-v12.cpp` should be made easier to replace without changing one-screen behavior.

Recommended direction:

- extract candidate-building / mapping-building helpers into a dedicated mapping runtime helper;
- keep the existing one-screen path producing byte-equivalent active mapping behavior;
- keep the same 0.25 s structural/layout refresh cadence initially;
- keep per-frame cursor containment cheap.

Do **not** add multi-screen behavior in the same commit if it makes the diff difficult to prove.

**Gate:** one-screen direct behavior and P4.1 mapping tests unchanged.

### Slice 3 — OBS candidate discovery by UUID

Replace `find_single_scene_display_capture(...)` ownership with discovery of top-level visible Display Capture candidates.

For each candidate capture, snapshot only bounded metadata needed by mapping:

- source UUID;
- display label/name for UI only;
- source ref during snapshot construction;
- physical monitor descriptor;
- scene item visibility;
- current box transform;
- crop;
- source dimensions;
- proven `SceneAxisAlignedMapping` or diagnostic reason.

Do not keep raw scene-item pointers across unsafe lifecycle boundaries. Re-enumerate structural state on refresh.

Persisted selection is UUID-based.

**Gate:** two-candidate discovery test seam + no camera behavior change yet.

### Slice 4 — Presentation Screens settings model

Define persisted eligibility separately from active runtime ownership.

Safe default:

- exactly one valid Display Capture: implicit Auto / no setup required;
- multiple valid Display Captures with no previous selection: **Needs setup**, do not guess all screens;
- provide `Select all visible Display Captures` as an explicit user action if useful;
- utility/OBS/chat display can remain unchecked.

Recommended storage:

- one OBS data array/list of selected source UUID strings;
- optional schema/version marker for migration.

Do not persist active screen every frame. Active screen is runtime state derived from cursor position.

**Gate:** restart/settings round-trip retains selected UUIDs; rename does not break selection when UUID is unchanged.

### Slice 5 — Active mapping adapter

Wire pure resolver result into the existing P4.1 synthetic descriptor path.

On each tick:

1. read current cursor coordinates once through existing input path;
2. choose candidate using the pure resolver;
3. if active candidate changes, select that candidate's already-proven mapping;
4. build/install the existing synthetic mapped `MonitorDescriptor`;
5. let inherited Smart Camera / click / cursor / Spotlight consume it normally.

Important:

- mapper change is **not** Zoom ON/OFF;
- mapper change must not reset camera kinematics;
- mapper change must not replay cinematic Spotlight;
- mapper change must not reset Spotlight resize state;
- mapper change must not write settings.

The existing `SceneViewportPlanner` should see a new scene-space target and own the travel naturally.

**Gate:** deterministic A→B→A trace proves no camera reset and no second planner.

### Slice 6 — Shared-consumer consistency test

Add an explicit integration seam/test proving the same active mapping is the source of truth for:

- Smart Follow target normalization;
- click anchor normalization;
- Presentation Cursor position;
- Spotlight Follow cursor / Click / Smart Focus mapping input.

Do not solve inconsistencies by adding per-feature resolvers.

**Gate:** one owner, four consumers.

### Slice 7 — Boundary stability

Only after direct traces show a real need, add minimal boundary hysteresis/debounce.

Preferred policy:

- physical monitor containment changes ownership immediately by default;
- add a tiny time/position guard only if border chatter is observable;
- never create a large dead zone that makes Monitor B feel unresponsive.

This is a tuning slice, not part of the initial architecture.

### Slice 8 — Beginner Properties UI + diagnostics

Target UX:

```text
Presentation Screens
☑ Display Capture — Coding (Monitor 1)
☑ Display Capture — Application (Monitor 2)
☐ Display Capture — OBS / Utility (Monitor 3)

Status: Active — Application / Monitor 2
```

Diagnostics should be event/state based:

- Ready — one Presentation Screen
- Ready — two Presentation Screens
- Active — Coding / Monitor 1
- Active — Application / Monitor 2
- Cursor outside Presentation Screens
- Needs setup — choose Presentation Screens
- Multiple captures claim the same monitor
- Presentation Screen missing/hidden
- Monitor unresolved
- Transform unsupported
- Geometry invalid

Avoid dynamic Properties rebuilds for rapidly changing active-screen status. Do not repeat the P5 GUI flicker bug. If live status cannot be updated safely without rebuilding, prefer a static settings view + log/diagnostic snapshot mechanism until a safe UI path is proven.

### Slice 9 — Direct OBS dual-screen acceptance

Required physical trial before promotion:

```text
Monitor 1 = Coding / IDE
Monitor 2 = Running app / browser
OBS scene = two captures side-by-side
```

Acceptance:

- Zoom ON while cursor on coding → coding region becomes clearly readable;
- move cursor to app → whole scene camera travels smoothly to app;
- move A↔B repeatedly → no snap/reset/border chatter;
- click ring / Classic Hand / Spotlight all land on active screen;
- Toggle Zoom cinematic remains correct;
- Zoom +/- remains resize-only;
- no black frame;
- no Properties flicker;
- no D3D11 unset-shader warnings;
- hide/delete/restore one capture fails safe;
- utility third display remains ignored when unchecked;
- selection survives OBS restart;
- source rename with stable UUID remains selected.

Only after this gate should #25 be considered stable-candidate material.

---

## 8. Performance budget for P4.2

P4.2 should remain cheap.

### Structural refresh path — infrequent

At ~4 Hz or event-driven later:

- enumerate top-level scene items;
- identify Display Capture candidates;
- resolve UUID / source properties / monitor;
- read transform/crop;
- rebuild mapping snapshots.

### Frame/tick path — O(number of selected screens)

Normal tutorial use is expected to be 1–3 presentation screens.

Per tick:

- one cursor sample;
- monitor rectangle containment checks;
- choose active snapshot;
- install existing synthetic mapping state.

No frame pixels are inspected.

Do not optimize with complexity before measuring. Keep a small benchmark/gate if resolver work grows.

---

## 9. P4.3 target architecture — Protected Overlays

P4.3 is a composition ownership problem, not a P4.2 mapper problem.

A post-composition filter cannot recover facecam/logo pixels as independent layers after OBS has already flattened them into the scene image.

Preferred first architecture to prove:

```text
Main Tutorial Scene
│
├─ Scene: Presentation Content
│      ├─ Display Capture — Coding
│      ├─ Display Capture — Application
│      ├─ zoomable browser/docs
│      └─ ArZoom Camera filter
│
├─ Facecam
├─ Facecam frame
├─ Logo
└─ Lower Third
```

OBS composes the child Presentation Content scene first. ArZoom transforms that child output. Then the parent scene adds fixed overlays.

This uses native OBS composition and preserves the no-runtime-transform-mutation rule.

---

## 10. P4.3 implementation slices

Do not begin P4.3 product automation until the manual native composition is proven.

### P4.3 Slice A — Manual architecture proof

Create a direct OBS test scene manually:

1. child scene `Presentation Content`;
2. coding + app captures inside child;
3. ArZoom Camera attached to child scene;
4. parent `Main Tutorial` contains child scene + facecam + logo.

Prove:

- child camera can zoom coding/app;
- parent facecam/logo remain fixed;
- #25 mapping works inside child coordinate space;
- disabling ArZoom restores child scene naturally;
- no transform recovery is required.

### P4.3 Slice B — Define v1 layout contract

For the first protected-overlay release, prefer a narrow proven contract:

- Presentation Content scene has the same base resolution/aspect as the parent tutorial scene;
- nested Presentation Content item is placed 1:1 / full-canvas in parent;
- fixed overlays live above it in parent.

Why: this keeps Spotlight/cursor output-space sizing predictable and avoids needing parent→child transform-chain compensation in the first release.

Scaled/rotated nested Presentation Content can be a later complex-layout increment.

### P4.3 Slice C — Setup diagnostics

ArZoom should detect/report common invalid setups:

- ArZoom Camera attached to parent when protected overlay workflow is intended;
- Presentation Content is not full-canvas/1:1 under the v1 contract;
- nested/per-source double ArZoom filters;
- missing Presentation Content layer;
- child scene has unsupported multi-screen geometry.

### P4.3 Slice D — Beginner setup workflow

Only after manual proof, evaluate a Tools workflow:

```text
Tools → ArZoom — Setup Tutorial Layout

Content ArZoom may zoom:
☑ Coding Display
☑ Application Display
☑ Browser docs

Keep fixed:
☑ Facecam
☑ Logo
☑ Lower Third

[Preview plan] [Apply]
```

Any automatic scene restructuring must be:

- explicit;
- previewable;
- user-approved;
- reversible;
- transaction-safe;
- based on normal visible OBS scenes/items, never hidden helpers.

Do not silently move sources behind the user's back.

### P4.3 Slice E — Direct acceptance

Test:

- coding/app multi-screen switching;
- facecam fixed bottom-right;
- logo fixed top-left;
- lower-third/browser overlay fixed;
- Toggle Zoom ON/OFF;
- Zoom +/-;
- Spotlight / Presentation Cursor;
- overlay hide/show/reorder;
- scene switch while zoomed;
- OBS restart;
- no black frame / no transform residue / no repair workflow.

---

## 11. Release sequencing

Recommended maturation path:

```text
v0.7.0  current stable
   ↓
P4.2 #25 Multi-Screen Smart Camera
   ↓ direct dual-screen acceptance
v0.8.0 candidate/stable
   ↓
P4.3 #26 Protected Overlays
   ↓ direct tutorial-layout acceptance
v0.9.0 candidate/stable
   ↓
Reliability / Setup Doctor / complex composition hardening
   ↓
v1.0 Windows North Star
```

Version numbers are planning guidance, not a release commitment. Do not bump version until feature acceptance and release closeout.

---

## 12. PR discipline

For #25:

- open Draft PR after the first real code/test slice, not an empty PR;
- PR body links #25 and this blueprint;
- list current exact regression count;
- mark each slice/gate as it lands;
- keep direct OBS acceptance unchecked until actually tried;
- do not merge because CI is green alone;
- squash merge only after direct acceptance and release-closeout sync.

Suggested commit style:

```text
P4.2: add pure presentation-screen resolver
P4.2: add deterministic resolver tests
P4.2: extract scene mapping candidate seam
P4.2: discover display captures by UUID
P4.2: persist presentation-screen eligibility
P4.2: install active screen mapping
P4.2: add multi-screen consistency gates
P4.2: add presentation-screen settings UX
P4.2: document direct OBS acceptance
```

Avoid commits like `fix stuff`, `try mapping`, or large mixed patches.

---

## 13. Stop conditions

Stop and diagnose before continuing if any slice causes:

- first-click / Zoom / cursor black frame;
- `device_draw (D3D11): Not all shader parameters were set`;
- OBS Properties flicker/rebuild on runtime actions;
- one feature using a different active screen than another;
- camera reset when cursor crosses screens;
- cinematic Spotlight replay on screen crossing;
- Zoom +/- replaying cinematic close/open;
- settings written every frame;
- scene transforms left modified after disable/crash;
- a need for a second renderer/planner to make the slice work.

Do not cover these symptoms with retries, warm frames, sampler hacks, or heuristic mapping.

---

## 14. Thread handoff protocol

A new implementation thread should start by reading, in this order:

1. `docs/TUTORIAL_CAMERA_IMPLEMENTATION_BLUEPRINT.md`
2. `docs/P4_1_STABLE_BASELINE.md`
3. `docs/P5_STABLE_BASELINE.md`
4. Issue #25 for P4.2 or Issue #26 for P4.3
5. `src/arzoom-scene-camera-core.hpp`
6. `src/arzoom-filter-v12.cpp`
7. `src/arzoom-filter-v24.cpp`
8. `tests/arzoom-phase41-scene-mapping-test.cpp`

Then verify current `main`/stable CI before creating a feature branch.

### New-thread starting instruction

Use this concise handoff:

> Continue ArZoom Tutorial Camera work using `docs/TUTORIAL_CAMERA_IMPLEMENTATION_BLUEPRINT.md` as the implementation source of truth. Preserve P4.1 and P5 stable invariants. For #25, begin with the next incomplete P4.2 slice only; do not redesign the renderer or camera. Run the new slice gate plus all existing regression tests before proceeding. Keep the PR Draft until direct OBS dual-screen acceptance.

### Mandatory status report after every slice

Report only:

- slice completed;
- files changed;
- invariant affected/not affected;
- exact tests run and result;
- current PR/head SHA if applicable;
- next single slice;
- blockers/evidence if any.

This keeps future threads deterministic and prevents context loss.

---

## 15. Final success definition

The combined Tutorial Camera direction is successful when a normal creator can produce this experience:

1. put coding on one display and the running application on another;
2. show both in one OBS tutorial layout;
3. work naturally with the mouse across both screens;
4. ArZoom makes the currently used work area clearly readable without manual camera operation;
5. facecam/logo/branding can remain stable when desired;
6. click, cursor and Spotlight stay spatially correct;
7. OBS scene composition remains recoverable by design because ArZoom does not continuously rewrite user transforms.

The product should feel like **a calm camera operator for technical tutorials**, not a mouse magnifier and not a scene-transform automation script.
