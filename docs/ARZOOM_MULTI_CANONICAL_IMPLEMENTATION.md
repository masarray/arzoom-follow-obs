# ArZoom Multi — Canonical Implementation Strategy

**Status:** authoritative implementation plan for Issue #25 / ArZoom Multi.

**Branch:** `feature/arzoom-multi-canonical`

**Product split:**
- existing stable filter: **ArZoom Single**;
- new multi-display filter: **ArZoom Multi**.

This document supersedes the failed P4.2 synthetic-monitor experiment in PR #27. It does **not** supersede the stable P4.1/P5 camera, motion, render, Spotlight, cursor, or click behavior. Those remain accepted baselines.

When another document conflicts with this file specifically about **how Issue #25 / ArZoom Multi is implemented**, this document wins until an explicitly reviewed architecture decision replaces it.

---

## 1. User outcome

A single OBS scene may contain multiple independently positioned/scaled Display Captures, for example:

```text
OBS canvas
┌────────────────────────────┬────────────────────────────┐
│ Display Capture A          │ Display Capture B          │
│ physical Monitor A         │ physical Monitor B         │
│ scaled/positioned in scene │ scaled/positioned in scene │
└────────────────────────────┴────────────────────────────┘
```

When the physical Windows cursor is on Monitor A, ArZoom Multi must map the cursor into **Display Capture A's local content coordinates**, then through **A's actual OBS scene transform**, and feed the resulting scene point into the one existing scene camera.

When the physical cursor moves to Monitor B, ownership switches to B and the same process uses **B's** source-local coordinates and scene transform.

The cursor must never be treated as if Windows desktop coordinates were already OBS-canvas coordinates.

---

## 2. Product boundary: Single and Multi are separate filters

### ArZoom Single

- user-facing name: `ArZoom Single`;
- existing internal OBS source/filter ID remains unchanged: `arzoom_filter`;
- existing v0.7.0/P4.1/P5 runtime remains the golden baseline;
- no ArZoom Multi topology worker, ownership resolver, or canonical multi-screen state is inserted into this filter;
- old scenes must continue to load because the internal ID is not renamed.

### ArZoom Multi

- user-facing name: `ArZoom Multi`;
- new internal OBS ID: `arzoom_filter_multi`;
- separate lifecycle/state owner for multi-display mapping;
- shares accepted camera/planner/kinematics/render primitives where appropriate, but does not reuse the Single filter's synthetic-monitor mapping contract as its source of truth.

### One scene, one camera authority

A scene may use **ArZoom Single XOR ArZoom Multi**, not both simultaneously.

If both are enabled on one scene, do not auto-disable user configuration. ArZoom Multi must fail safe/pass through and present a clear camera-authority conflict diagnostic.

---

## 3. Retired architecture — do not revive

The PR #27 experiment extended the Single/P4.1 seam using a synthetic/mapped monitor and continued to drive inherited `phase1->monitor` normalization.

Direct OBS 32.1.2 testing repeatedly failed the core two-display requirement: the second physical display/source did not reliably acquire cursor ownership.

The following design is therefore retired for Multi:

```text
physical cursor
→ synthetic desktop/mapped monitor
→ phase1->monitor
→ inherited single-screen normalization
```

Useful tests and identity lessons from PR #27 may be reused, but its runtime wrapper stack is not a production foundation.

Do not add a fourth/fifth patch to that architecture.

---

## 4. Canonical coordinate contract

ArZoom Multi has exactly one coordinate chain:

```text
PHYSICAL CURSOR
      ↓
PHYSICAL DISPLAY OWNERSHIP
      ↓
ELIGIBLE DISPLAY CAPTURE UUID
      ↓
SOURCE-LOCAL UV
      ↓
THAT SOURCE'S OBS SCENE TRANSFORM
      ↓
ONE CANONICAL SCENE POINTER
      ↓
SceneViewportPlanner
      ↓
SceneKinematicMotion
      ↓
ONE scene-level camera
```

Definitions:

1. **Physical cursor** — one platform point in one explicitly chosen coordinate space.
2. **Physical display** — deterministic physical monitor rectangle in that same coordinate space.
3. **Presentation Screen** — a user-eligible Display Capture identified durably by source UUID.
4. **Source UV** — cursor position normalized to the captured display/content domain, normally `[0,1] × [0,1]` after the supported crop model is applied.
5. **Scene transform** — deterministic source-local/content → OBS scene mapping for that exact Display Capture scene item.
6. **Canonical scene pointer** — the one scene-space pointer snapshot consumed by all pointer-driven presentation features.

No consumer may independently rediscover a monitor, source, or scene mapping.

---

## 5. Canonical data model

Prefer compact POD/value state. Runtime hot paths must not own OBS object graphs.

Suggested shape:

```cpp
constexpr size_t kMaxPresentationScreens = 8;

enum class MultiPointerStatus : uint8_t {
    Ready,
    CursorOutside,
    NoEligibleScreens,
    AmbiguousPhysicalOwner,
    SourceUnavailable,
    UnsupportedGeometry,
    TopologyStale,
    CameraAuthorityConflict,
};

struct RawPresentationScreen {
    SourceUuid uuid;
    PhysicalRect monitor_rect;
    uint32_t source_width;
    uint32_t source_height;
    Crop crop;
    RawSceneTransform scene_transform;
    bool visible;
    bool eligible;
};

struct RawTopologySnapshot {
    uint64_t generation;
    uint8_t count;
    RawPresentationScreen screens[kMaxPresentationScreens];
};

struct CanonicalPresentationScreen {
    SourceUuid uuid;
    PhysicalRect monitor_rect;
    CanonicalSourceToScene transform;
    ScreenStatus status;
};

struct CanonicalTopology {
    uint64_t generation;
    uint8_t count;
    CanonicalPresentationScreen screens[kMaxPresentationScreens];
};

struct CanonicalScenePointer {
    uint64_t topology_generation;
    uint8_t active_screen;
    float source_u;
    float source_v;
    float scene_x;
    float scene_y;
    MultiPointerStatus status;
};
```

Exact C++ names may change, but the ownership model must not.

---

## 6. Coordinate-space rule

For Windows v1, choose **one canonical physical-pixel coordinate contract** end-to-end for cursor ownership and monitor rectangles. Do not mix Qt device-independent coordinates, DPI-virtualized coordinates, physical monitor pixels, and OBS source pixels without explicit conversion.

Before production camera wiring, a diagnostic/probe milestone must prove on real hardware that:

```text
physical cursor → physical monitor → source UUID → source UV → scene point
```

is correct for both displays.

Mixed-DPI and negative virtual-desktop coordinates are mandatory test cases.

If the Windows backend cannot prove that two values are expressed in the same coordinate space, it must not compare them.

---

## 7. Scene transform model

The Multi mapper must operate source-first, never canvas-first.

Conceptually:

```text
physical monitor point
      ↓ normalize
source/content UV
      ↓ crop-visible-domain mapping
source-local point
      ↓ OBS scene-item transform
scene point
```

Release-v1 support should be intentionally conservative:

- translation: supported;
- positive axis-aligned scale: supported;
- independent X/Y scale: supported when math is proven;
- inset/positioned sources: supported;
- deterministic crop: supported only after dedicated tests;
- rotation/skew/flips/unsupported bounds modes: fail safe initially.

Internally, prefer a canonical affine representation even if v1 rejects non-axis-aligned transforms. That avoids redesigning the state model later.

Never add screenshot-specific offsets or monitor-order heuristics.

---

## 8. Threading model

Worker threads are used only where they remove structural work from realtime paths. Do **not** move cheap per-frame pointer math to a worker.

### OBS/control/main-thread responsibilities

- enumerate relevant scene items and Display Captures;
- acquire source UUIDs and settings;
- read crop/bounds/scene transform state;
- observe source/scene lifecycle events;
- observe physical display topology changes through the platform backend;
- copy all required data into a bounded `RawTopologySnapshot`;
- never publish partially built topology.

### One topology preparation worker

A single owned worker may:

- validate raw topology;
- normalize monitor/source geometry;
- precompute source→scene coefficients/matrices;
- build diagnostics codes;
- produce one immutable `CanonicalTopology` candidate.

The worker must not:

- call arbitrary OBS frontend/scene APIs;
- retain `obs_source_t*`, `obs_sceneitem_t*`, Qt UI objects, or other lifecycle-sensitive runtime pointers;
- render;
- poll the cursor;
- write settings/files;
- accumulate an unbounded job queue.

### Video-tick responsibilities

Steady-state video tick does only bounded work:

```text
read published topology
read physical cursor
resolve owning screen
calculate source UV
apply prepared source→scene transform
publish CanonicalScenePointer
step existing camera/planner/kinematics
```

No scene enumeration, settings lookup, matrix inversion, string formatting, file access, or allocation is allowed in this steady-state path.

### Render-thread responsibilities

Keep the accepted GPU path. Multi mapping must not introduce:

- extra full-scene render passes;
- CPU readback;
- texture creation/destruction per frame;
- shader compilation per frame.

---

## 9. Coalescing / latest-wins worker contract

Topology change bursts must be coalesced.

Wrong:

```text
change 101 → queue
change 102 → queue
change 103 → queue
...
worker rebuilds every intermediate state
```

Required:

```text
requested_generation = latest
one pending slot
worker builds latest available generation
stale completed candidate is not published
```

Rules:

- queue depth is logically one pending request;
- a newer generation supersedes older unstarted work;
- if generation N finishes after N+1 was requested, N may be discarded and N+1 built next;
- publication is transactional: validate → prepare → verify → publish;
- failed preparation retains the last-known-good canonical topology only when that fallback is semantically safe; otherwise publish a fail-safe unavailable status;
- shutdown joins the worker; no detached thread.

Idle worker CPU should be effectively zero.

---

## 10. Snapshot publication / hot-read model

Realtime readers must not block on a long mutex held by topology preparation.

Preferred model: bounded double-buffer or sequence-lock style publication of trivially copyable prepared state.

Required properties:

- one writer;
- lock-free or extremely short bounded read;
- reader either sees old complete state or new complete state, never a torn topology;
- generation is part of the published snapshot;
- no heap ownership handoff in the video/render hot path.

Exact implementation may use a double buffer + atomic published index/generation if simpler and easier to prove than a custom seqlock.

Choose the simplest mechanism that provides coherent snapshots and test it under rebuild storms.

---

## 11. Event-driven invalidation

Do not continuously re-enumerate the scene every video frame or on a permanent 250 ms rebuild loop.

Mark topology dirty on relevant structural changes, including as available:

- Display Capture add/remove;
- source show/hide;
- selected Presentation Screens changed;
- scene-item position/scale/crop/bounds changed;
- Display Capture monitor selection changed;
- scene switch / filter attach lifecycle;
- monitor connect/disconnect / display topology change;
- OBS restart/load.

All dirty events coalesce into the latest requested generation.

A low-frequency safety audit/watchdog is allowed only as a recovery mechanism for missed notifications; it must fingerprint state and avoid rebuild when nothing changed.

---

## 12. Shared canonical pointer consumers

`CanonicalScenePointer` is the only live pointer mapping result.

It feeds:

- Smart Follow / `SceneViewportPlanner`;
- click capture anchoring;
- Presentation Cursor;
- Spotlight Follow Cursor;
- Spotlight Smart Focus live pointer;
- any future pointer-driven presentation primitive.

Historical click events capture scene/content position at event time and remain stable afterward; they are not remapped through the newly active display after a monitor switch.

No presentation consumer may call the physical-display resolver independently.

---

## 13. Hotkey / command routing

Public presenter commands remain one ArZoom command set. Users should not manage duplicate Single/Multi hotkeys.

The command layer resolves the one active camera authority for the current scene:

- exactly one valid `ArZoom Multi` → route to Multi;
- otherwise exactly one valid `ArZoom Single` → route to Single;
- conflicting active authorities → fail safe + diagnostic;
- never send one command to two camera authorities.

Toggle Zoom cinematic and Zoom +/- resize-only semantics remain P5-stable behavior.

---

## 14. Diagnostics architecture

Diagnostics are state/event based, not per-frame strings.

Hot paths emit compact codes/counters only, for example:

```cpp
struct MultiDiagnosticEvent {
    MultiDiagnosticCode code;
    uint8_t screen_index;
    uint32_t generation;
    int32_t detail_a;
    int32_t detail_b;
};
```

Requirements:

- bounded storage;
- duplicate state transitions coalesce;
- repeated identical failures increment a counter rather than append endlessly;
- human-readable formatting happens outside render/video hot paths;
- diagnostics consumer failure cannot block camera/rendering;
- no network telemetry requirement.

Beginner wording should say things such as:

- `Ready — 2 presentation displays`;
- `Active — Display Capture 2 / Monitor 2`;
- `Cursor outside presentation displays`;
- `Display Capture 2 cannot be matched to a physical display`;
- `Unsupported source transform`;
- `ArZoom Single is already active on this scene`.

---

## 15. Performance budgets

These are design targets and must be measured before promotion:

| Metric | Target |
|---|---:|
| steady-state heap allocations per video tick | 0 |
| OBS scene/source enumeration per video tick | 0 |
| settings/file writes per video tick | 0 |
| blocking locks in render callback | 0 |
| CPU frame readback | 0 |
| additional full-scene GPU render passes | 0 |
| topology job backlog | 0; latest-wins only |
| presentation screens | bounded, max 8 initially |
| diagnostic state | bounded |
| idle worker CPU | effectively 0 |
| per-frame mapping complexity | fixed small-N / bounded |

Do not call an optimization successful without benchmark evidence.

Benchmarks should cover at least 1, 2, 4, and 8 screens; repeated A↔B ownership switching; topology rebuild storms; and idle/no-change behavior.

---

## 16. Milestone plan

Implementation is one reversible milestone at a time. Do not stack later milestones before the current direct gate passes.

### M0 — Dual-filter registration boundary

Goal:
- existing `arzoom_filter` displays as **ArZoom Single** without changing its internal ID/runtime behavior;
- new `arzoom_filter_multi` registers as **ArZoom Multi**;
- Multi is pass-through/no camera behavior initially;
- camera-authority conflict detector exists;
- stable Single tests/build remain green.

Direct gate:
- old OBS scene still loads Single;
- filter list visibly contains Single and Multi;
- adding Multi alone does not black-frame or modify scene behavior.

### M1 — Pure canonical coordinate engine

Goal:
- platform-neutral pure math for physical rect → source UV → source/crop domain → scene transform;
- no OBS runtime wiring;
- synthetic-monitor concept absent.

Tests:
- two scaled side-by-side captures;
- unequal source sizes;
- inset layouts;
- negative monitor coordinates;
- edges/corners and half-open ownership;
- invalid/ambiguous ownership;
- crop cases only when supported;
- unsupported transform rejection.

### M2A — Pure topology/worker/publication core

Goal:
- establish bounded value-only raw/canonical topology types;
- implement one latest-wins worker with one pending slot;
- prove stale generations cannot publish;
- prove deterministic join/shutdown;
- prove coherent fixed double-buffer publication with no reader mutex;
- **no OBS runtime wiring yet**.

Tests:
- 1,000+ dirty notifications collapse to active + latest only;
- stale generation never overwrites newer state;
- older/out-of-order generations are ignored;
- shutdown during rebuild is safe;
- no queue growth;
- invalid latest state publishes fail-safe/unavailable state;
- concurrent publication never exposes torn state.

M2A may be completed while an earlier direct boundary gate is pending only if it remains pure and unused by OBS runtime.

### M2B — OBS raw topology capture + worker integration

Goal:
- OBS/control thread produces the bounded raw topology consumed by M2A;
- source UUID/visibility/source size/crop/scene geometry enter value-only snapshots;
- relevant structural events mark topology dirty;
- M2A worker starts/stops with ArZoom Multi lifecycle;
- immutable canonical topology is published coherently;
- Multi remains visually pass-through; camera still does not move.

M2B is a runtime milestone and must not begin while a required earlier direct OBS boundary gate is unresolved.

### M3 — ScenePointer diagnostic probe

Goal:
- **camera still does not move**;
- Properties/diagnostics prove live mapping on real hardware:

```text
Physical Monitor 2
→ Display Capture 2 UUID
→ source UV (u,v)
→ scene (x,y)
```

Direct OBS gate on target OBS versions:
- cursor center/edges of Monitor A maps into source A scene region;
- cursor center/edges of Monitor B maps into source B scene region;
- scaled side-by-side source layout is correct;
- mixed DPI and negative coordinate setup tested where available.

**STOP if M3 is not correct. Do not connect camera as a workaround.**

### M4 — Camera-only Multi

Goal:
- feed canonical scene pointer to the accepted `SceneViewportPlanner` and `SceneKinematicMotion`;
- one camera authority;
- monitor crossing changes target, not camera state ownership;
- no Spotlight/cursor/click Multi wiring yet.

Direct gate:
- A↔B repeatedly follows correct scene regions smoothly;
- no snap/reset/cinematic replay;
- no boundary chatter requiring guesswork.

### M5 — Shared presentation consumers

Goal:
- click, Presentation Cursor, Spotlight live modes consume the same canonical pointer/event seam;
- historical click stays anchored after monitor switch.

Direct gate:
- all effects land on correct source/scene location on A and B.

### M6 — Production UX, persistence, Setup Doctor

Goal:
- UUID-based Presentation Screen selection;
- beginner Properties UI;
- restart persistence;
- rename same UUID preserved;
- delete/recreate new UUID does not inherit;
- source missing/hidden/reconnected states clear and fail safe;
- Single/Multi conflict diagnostic.

### M7 — Performance, compatibility, acceptance closeout

Required:
- deterministic full regression suite;
- benchmark budgets recorded;
- Windows build/package;
- CI against stable supported OBS and current next-major OBS (including OBS 32.x while it is current);
- direct dual/multi-monitor OBS acceptance;
- 30/60/120/144 fps stress;
- mixed DPI and negative coordinates;
- monitor disconnect/reconnect and restart;
- Intel/AMD/NVIDIA coverage as available;
- no black-frame/D3D11 shader regression.

Only after M7 may Issue #25 be promoted to stable-candidate documentation/release work.

---

## 17. Stop conditions

Stop immediately and root-cause before proceeding when:

- physical cursor→display ownership cannot be proven;
- source UUID→scene item identity is ambiguous;
- source-local→scene math differs from direct probe evidence;
- a worker requires OBS object calls off the owning thread;
- publication can expose partial/torn topology;
- a feature starts resolving its own Display Capture independently;
- Single behavior changes unintentionally;
- black frame or D3D11 shader warnings appear;
- a patch would be the third/fourth symptom patch in the same subsystem without architecture review.

Never weaken an existing stable P0–P5 gate just to make Multi pass.

---

## 18. Forbidden implementation shortcuts

ArZoom Multi must not introduce:

- per-source zoom engines;
- a second semantic `SceneViewportPlanner`;
- persistent `obs_sceneitem_set_*` camera mutation;
- private duplicate/off-screen scene rendering;
- hidden helper scene/source for camera implementation;
- CPU frame readback;
- OCR/vision for monitor/source ownership;
- nearest/largest source guessing;
- source-name or scene-item-order persistent identity;
- runtime pointer identity as persistence;
- per-frame settings writes;
- unbounded queues/histories;
- per-frame scene enumeration;
- per-frame diagnostics formatting/log spam;
- detached worker threads;
- arbitrary sleep/delay race fixes.

---

## 19. Validation hierarchy

Every milestone must distinguish evidence levels:

1. **Pure deterministic tests** — math/state contract.
2. **Regression suite** — protects stable Single/P0–P5 behavior.
3. **Windows C++/shader build** — compile/package proof.
4. **Performance/resource check** — hot-path cost proof where changed.
5. **Direct OBS validation** — final truth for runtime mapping/render/user behavior.

CI green is never a substitute for the required direct OBS gate.

---

## 20. Definition of done for ArZoom Multi

Issue #25 is done only when all are true:

- ArZoom Single remains compatible with existing scenes and stable behavior;
- ArZoom Multi is a distinct filter/runtime authority;
- two or more selected physical displays resolve deterministically;
- independently scaled/positioned Display Captures map physical cursor → source local → scene correctly;
- one canonical scene pointer feeds camera/click/cursor/Spotlight;
- crossing monitors reuses existing planner/kinematics without camera reset;
- failure/ambiguity is visible and safe, never guessed;
- worker/coalescing/publication contracts are bounded and lifecycle-safe;
- steady-state hot path meets measured performance targets;
- OBS stable + next-major compatibility gates are green;
- direct two-monitor and stress acceptance is explicitly accepted by the project owner;
- public/stable documentation is updated only after that acceptance.

---

## 21. Required continuation discipline

Every implementation session must begin by reading:

1. `AGENTS.md`;
2. `docs/PROJECT_DIRECTION.md`;
3. `docs/P4_1_STABLE_BASELINE.md`;
4. `docs/P5_STABLE_BASELINE.md`;
5. this file;
6. `docs/ARZOOM_MULTI_HANDOFF.md`;
7. Issue #25 and the current canonical Draft PR.

Then implement **only the `Next ONE milestone` recorded in the handoff file**.

At the end of every milestone, update `docs/ARZOOM_MULTI_HANDOFF.md` in the same commit or immediately following documentation commit so another thread can continue without hidden conversational context.
