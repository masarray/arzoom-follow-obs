# ArZoom Multi — Living Handoff Ledger

**Purpose:** make Issue #25 resumable from a fresh ChatGPT/thread/agent session without relying on hidden conversation history.

**Authority rule:** for Issue #25 implementation, this file is the current execution ledger and `docs/ARZOOM_MULTI_CANONICAL_IMPLEMENTATION.md` is the architecture source of truth. If chat memory disagrees with the repository, the repository wins.

**Scope rule:** Issue #25 only. Issue #26 remains out of scope.

---

## 1. Current canonical status

- Repository: `masarray/arzoom-follow-obs`
- Product tracker: **Issue #25 — Multi-Screen Smart Camera**
- Canonical branch: `feature/arzoom-multi-canonical`
- Canonical Draft PR: **#34 — ArZoom Multi: canonical architecture + milestone implementation ledger**
- Base branch: `main`
- Reset base SHA: `ada8f5269246c64429d7aceb6cc72f81e72120ba`
- Stable public baseline: **ArZoom v0.7.0**
- Stable camera/mapping baseline: **P4.1**
- Stable presentation baseline: **P5**
- Retired failed experiment: **PR #27 — do not resume**
- Old CI-only PR #32: closed, no product scope
- Current latest runtime implementation SHA: **`750600607c920078a75f8fbe2e37db3b54ec7cb6`**
- Runtime commit message: **`feat(multi): add M1 canonical coordinate engine`**
- The handoff/documentation commit follows the runtime SHA. A new thread must always re-fetch PR #34 and verify its actual current head before editing.

### Milestone state

| Milestone | Status | Evidence |
|---|---|---|
| Strategy reset / canonical docs | COMPLETE | PR #34, Build Windows #284 PASS |
| M0 Dual-filter registration | **IMPLEMENTED / CI PASS / DIRECT OBS TRIAL PENDING** | runtime `882053b412f1b2320fe798af313b3f14467c2296`; Build #285 PASS; current-head docs Build #286 PASS |
| M1 Pure canonical coordinate engine | **COMPLETE / CI PASS / PURE-MATH ONLY** | runtime `750600607c920078a75f8fbe2e37db3b54ec7cb6`; Build #287 PASS; 21/21 CTest |
| M2 Raw topology capture + coalescing worker | BLOCKED / NOT STARTED | blocked by pending M0 direct OBS boundary acceptance |
| M3 ScenePointer diagnostic probe | NOT STARTED | camera must still not move |
| M4 Camera-only Multi | NOT STARTED | blocked until M3 physical mapping proof |
| M5 Shared click/cursor/Spotlight consumers | NOT STARTED | — |
| M6 UX/persistence/Setup Doctor | NOT STARTED | — |
| M7 Performance/compatibility/direct acceptance | NOT STARTED | — |

### Next ONE action

> **Complete M0 direct OBS filter-list/pass-through acceptance using the current M1 package.**

M1 changes are header-only pure math and are not wired into the Multi runtime, so the Build #287 package is valid for the same M0 filter-list/pass-through trial.

**Do not begin M2 until that direct M0 boundary trial is recorded PASS.**

---

## 2. Mandatory read order for every fresh thread

Before any new implementation, read in this exact order:

1. `AGENTS.md`
2. `docs/PROJECT_DIRECTION.md`
3. `docs/P4_1_STABLE_BASELINE.md`
4. `docs/P5_STABLE_BASELINE.md`
5. `docs/ARZOOM_MULTI_CANONICAL_IMPLEMENTATION.md`
6. **this file**
7. Issue #25
8. Draft PR #34

Then:

1. fetch PR #34 again;
2. verify branch and exact head SHA;
3. inspect CI status for that exact head;
4. implement only the **Next ONE action/milestone** recorded here;
5. update this ledger before stopping.

Never rely on “the previous assistant remembers it.”

---

## 3. User outcome / coordinate truth

The important case is one OBS scene containing two or more independently transformed Display Captures:

```text
Physical desktop

Monitor A                         Monitor B
┌──────────────────┐              ┌──────────────────┐
│ physical cursor  │              │ physical cursor  │
└──────────────────┘              └──────────────────┘
          │                                │
          ▼                                ▼
Display Capture A                  Display Capture B
          │                                │
          └──────────────┬─────────────────┘
                         ▼
OBS scene/canvas

┌────────────────────────┬────────────────────────┐
│ A scaled/positioned    │ B scaled/positioned    │
│ independently          │ independently          │
└────────────────────────┴────────────────────────┘
```

ArZoom Multi must use exactly this semantic chain:

```text
physical cursor
→ deterministic physical display ownership
→ eligible Display Capture UUID
→ full-source/source-local UV
→ visible crop domain
→ that exact source's prepared source→scene transform
→ ONE CanonicalScenePointer
→ existing SceneViewportPlanner
→ existing SceneKinematicMotion
→ ONE scene-level ArZoom camera
```

Physical Windows desktop coordinates are **not** OBS canvas coordinates.

---

## 4. Frozen product decisions

### Filter identity

- Stable filter display name: **ArZoom Single**
- Stable internal OBS ID: **`arzoom_filter`**
- New filter display name: **ArZoom Multi**
- New internal OBS ID: **`arzoom_filter_multi`**

The legacy internal ID must never be renamed because existing OBS projects persist it.

### Single is frozen

ArZoom Single is the golden P4.1/P5 reference. Do not insert Multi ownership, worker, canonical topology, or source-selection logic into Single.

### One camera authority

A source/scene may have **Single XOR Multi** as camera authority. Multi must fail safe/pass through if authority is ambiguous. It must not silently auto-disable/delete/reorder user filters.

### Durable screen identity

Presentation Screen persistence will use **source UUID**.

Never persist identity by:

- source display name;
- scene-item order/index;
- runtime pointer;
- “first/nearest/largest” source.

Rename with the same UUID preserves identity. Delete/recreate with a new UUID is a new screen identity.

### No guessing

Missing/ambiguous/unsupported geometry pauses pointer retargeting and exposes a diagnostic state. Never approximate merely to keep the camera moving.

---

## 5. Retired architecture — PR #27

The failed P4.2 experiment used:

```text
physical cursor
→ physical monitor
→ synthetic/mapped monitor
→ inherited phase1->monitor normalization
→ scene camera
```

Direct OBS 32.1.2 testing repeatedly showed that Screen 2 did not reliably acquire ownership. A later monitor-ID patch still failed physical retest.

Therefore:

- PR #27 remains only failure evidence/test-idea history;
- do not reopen its synthetic-monitor wrapper stack;
- do not patch that path a fourth/fifth time;
- Multi is source-first: **physical → source-local → scene**.

---

## 6. Canonical runtime architecture target

```text
OBS/control thread
   capture bounded RawTopologySnapshot
               │
               ▼
one owned topology-preparation worker
latest-wins / coalescing / no FIFO backlog
               │
               ▼
validate + precompute source→scene transforms
               │
               ▼
immutable CanonicalTopology
double-buffer / atomic publication
               │
               ▼
video tick — bounded hot path
physical cursor
→ owner
→ source UV
→ scene point
               │
               ▼
ONE CanonicalScenePointer
       ┌───────┼────────┬────────┐
       ▼       ▼        ▼        ▼
    Camera    Click    Cursor   Spotlight
```

No consumer may rediscover a monitor/source/mapping independently.

---

## 7. Performance and worker contract

### Steady-state target

```text
heap allocations / video tick     = 0
scene enumeration / video tick    = 0
settings writes / video tick      = 0
file I/O / video tick             = 0
blocking render locks             = 0
CPU frame readback                = 0
extra full-scene render pass      = 0
topology FIFO backlog             = 0
```

Initial Presentation Screen cap: **8**.

### M2 worker contract

Use one owned topology-preparation worker, not a general pool.

It may:

- validate value-only raw snapshots;
- normalize geometry;
- precompute canonical coefficients;
- build bounded diagnostic codes;
- publish one immutable prepared topology.

It must not:

- retain `obs_source_t*`, `obs_sceneitem_t*`, Qt objects, or other lifecycle pointers;
- call arbitrary scene/frontend APIs off the owning thread;
- render;
- poll cursor;
- write settings/files;
- accumulate unbounded jobs.

Latest-wins behavior:

```text
requested 101,102,103,104
→ one pending/latest slot
→ obsolete unstarted generations disappear
→ stale completed generation never overwrites newer requested state
→ build/publish latest coherent generation
```

Shutdown must join deterministically.

Preferred publication: fixed double buffer + atomic published index/generation. Reader sees either a complete old snapshot or a complete new snapshot, never torn state.

---

# IMPLEMENTATION RECORD

## 8. M0 — Dual-filter registration boundary

### Runtime commit

`882053b412f1b2320fe798af313b3f14467c2296`

`feat(multi): establish M0 dual-filter boundary`

### Files changed

- `CMakeLists.txt`
- `src/arzoom-filter-boundary.hpp` — new
- `src/arzoom-filter-multi.cpp` — new
- `src/plugin-main.cpp`
- `tests/CMakeLists.txt`
- `tests/arzoom-m0-filter-boundary-test.cpp` — new

### Runtime architecture

```text
arzoom_filter
└─ ArZoom Single
   └─ accepted stable v0.7.0/P4.1/P5 runtime

arzoom_filter_multi
└─ ArZoom Multi
   └─ M0 pass-through shell only
```

M0 Multi deliberately has:

- no canonical mapper;
- no topology worker;
- no camera movement;
- no Presentation Screen persistence;
- no click/cursor/Spotlight Multi wiring;
- no dynamic Properties surface.

Its render callback uses `obs_source_skip_video_filter()`.

### Conflict guard

M0 watches parent `filter_add` / `filter_remove` structural events and only rescans when filter topology changes.

Conflict states:

- Single + Multi on same source;
- duplicate Single;
- duplicate Multi.

No per-frame enumeration/log spam. No automatic mutation.

### M0 CI evidence

Build Windows **#285** on exact runtime head `882053b4...`: **PASS**

- CTest: **20/20 PASS**
- `arzoom-m0-dual-filter-boundary`: PASS
- all existing P0–P5/P4.1 gates: PASS
- OBS Studio **31.1.1 x64** plugin compile: PASS
- `arzoom.dll`: PASS
- ZIP: PASS
- Inno Setup installer: PASS
- artifact upload: PASS
- Windows package artifact ID: `10349797091`

Documentation-only handoff head then passed Build Windows **#286**.

### M0 direct evidence

**PENDING.**

Do not write “M0 ACCEPTED” until a real OBS trial proves the filter-list/loading/pass-through boundary.

---

## 9. Controlled gate deviation authorized on 2026-09-28

The prior ledger said “do not start M1 before M0 direct acceptance.”

The project owner subsequently explicitly instructed: **continue next progress**.

A limited deviation was taken to avoid wasting development time while preserving runtime safety:

- M1 was allowed to proceed because it is **pure platform-neutral math/state only**;
- M1 is not called from OBS runtime;
- M1 does not alter Single;
- M1 does not alter Multi pass-through behavior;
- no worker/camera/topology integration was added;
- **M0 remains unaccepted**;
- **M2 remains blocked until M0 direct boundary trial passes**.

This is intentional and documented. A future thread must not reinterpret M1 CI success as proof that M0 passed direct OBS acceptance.

---

## 10. M1 — Pure canonical coordinate engine

### Runtime commit

`750600607c920078a75f8fbe2e37db3b54ec7cb6`

`feat(multi): add M1 canonical coordinate engine`

Parent: `c4d4331329b440760124f82e36059f5ea90341e4`

### Files changed

1. `src/arzoom-multi-coordinate.hpp` — new pure engine
2. `tests/arzoom-m1-coordinate-engine-test.cpp` — new deterministic gate
3. `tests/CMakeLists.txt` — registers M1 test
4. `CMakeLists.txt` — lists the new header in the plugin target

No OBS runtime source file calls the M1 resolver yet.

### M1 ownership/state model

The pure engine defines bounded, value-only state:

- `kMultiMaxPresentationScreens = 8`
- `MultiPhysicalRect` using signed 64-bit desktop coordinates
- `MultiCrop`
- `MultiSceneRect`
- `MultiAffine2D`
- `MultiPresentationScreen`
- `MultiScenePointer`
- `MultiGeometryStatus`
- `MultiPointerStatus`

`MultiPresentationScreen` and `MultiScenePointer` are compile-time asserted trivially copyable so they remain suitable for later bounded publication.

### Canonical transform contract

The affine transform maps **full-source pixels → scene pixels**:

```text
scene_x = xx * source_x + xy * source_y + tx
scene_y = yx * source_x + yy * source_y + ty
```

M1 release-v1 acceptance intentionally allows only:

- finite positive X scale;
- finite positive Y scale;
- translation;
- independent X/Y scale;
- crop-aware visible domain.

M1 rejects:

- rotation;
- skew;
- flips;
- degenerate/non-finite transform;
- exhaustive/invalid crop;
- invalid source dimensions;
- invalid physical monitor rectangle.

The affine representation remains general so later support can expand without changing the canonical snapshot shape.

### Ownership contract

Physical monitor rectangles are half-open:

```text
left <= x < right
top  <= y < bottom
```

This gives deterministic adjacent-monitor ownership at boundaries.

Resolver policy:

- zero eligible screens → `NoEligibleScreens`
- cursor outside all eligible valid screens → `CursorOutside`
- two eligible physical rects claim cursor → `AmbiguousPhysicalOwner`
- unsupported owner geometry → `UnsupportedGeometry`
- cursor falls in a cropped-away part of the captured source → `CursorOutsideVisibleSource`
- >8 screens → `TooManyScreens`
- no nearest/largest/first fallback

### Physical coordinate precision

Normalization uses signed 64-bit rectangles and `long double` intermediate subtraction/division before returning canonical double UV. This avoids signed subtraction overflow even with extreme negative/positive virtual desktop coordinates.

### M1 deterministic gates

`tests/arzoom-m1-coordinate-engine-test.cpp` proves:

1. two independently positioned/scaled side-by-side screens;
2. unequal source sizes;
3. source-center UV correctness on A and B;
4. independent scene center mapping;
5. negative desktop coordinates;
6. adjacent half-open A→B boundary ownership;
7. cursor outside eligible screens never guesses;
8. overlapping monitor ownership is ambiguous;
9. unchecked utility screen never becomes active;
10. zero eligible screens fails safe;
11. valid crop maps full-source coordinates into the visible scene box;
12. cropped-away pointer region is rejected;
13. exhaustive crop is rejected;
14. rotation/skew-like affine is rejected;
15. flip/negative scale is rejected;
16. invalid source size fails safe;
17. invalid physical monitor rect fails safe;
18. Presentation Screen cap >8 is rejected before scanning;
19. extreme signed desktop coordinate normalization does not overflow.

### M1 local preflight

A contract-equivalent local compile was run with:

```text
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror
```

Result:

```text
ArZoom M1 canonical coordinate engine gates: PASS
```

### M1 CI evidence

Build Windows **#287**

Exact head:

`750600607c920078a75f8fbe2e37db3b54ec7cb6`

Result: **PASS**

Evidence:

- CTest: **21/21 PASS**
- `arzoom-m1-canonical-coordinate-engine`: PASS
- all existing P0–P5/P4.1/M0 gates: PASS
- OBS Studio **31.1.1 x64** plugin compile: PASS
- `arzoom-filter-multi.cpp`: compiled
- `arzoom.dll`: PASS
- ZIP package: PASS
- Inno Setup installer: PASS
- Windows artifact upload: PASS
- Windows package artifact ID: **`10958317003`**
- Windows artifact digest: **`sha256:8b85618888c55096df2e2538be63ea73e992572a4a8c9f8bcc0e6fc12d817e00`**
- benchmark artifact ID: `10957489290`

### M1 performance/hot-path impact

**Zero runtime hot-path impact.**

The header is not wired into Multi or Single runtime. There is:

- no new video tick;
- no worker;
- no scene enumeration;
- no settings I/O;
- no render change;
- no camera/planner call;
- no allocation introduced into the running plugin by M1.

No performance improvement claim is made.

### M1 direct OBS evidence

Not applicable to the pure math itself because it is not runtime-wired.

However, the overall branch still has the unresolved **M0 direct OBS boundary gate**.

---

# CURRENT BLOCKER / NEXT ACTION

## 11. M0 direct filter-list/pass-through acceptance

Use the current Build #287 package. M1 is pure-only, so runtime behavior is still the M0 pass-through boundary.

Required direct observations:

1. Install the current package in the user's real OBS.
2. Open an existing project that already contains the legacy ArZoom filter.
   - It must load normally.
   - Existing behavior must remain the same.
   - Type is presented as **ArZoom Single**.
3. Open **Filters → +**.
   - **ArZoom Single** is visible.
   - **ArZoom Multi** is separately visible.
4. Add **ArZoom Multi** alone.
   - image remains visually unchanged;
   - no zoom/follow behavior occurs;
   - no black frame.
5. Put Single + Multi on the same source.
   - OBS remains stable;
   - log contains one conflict transition, not per-frame spam.
6. Remove the conflict.
   - one conflict-cleared transition appears.
7. Confirm:
   - no crash;
   - no black frame;
   - no Properties flicker;
   - no regression in existing Single controls.

If any item fails:

> **STOP on M0. Fix only the boundary defect. Do not begin M2.**

If all pass:

1. update this file: M0 → **ACCEPTED**
2. record OBS version and evidence
3. set **Next ONE milestone = M2**
4. begin M2 only after the acceptance commit is pushed

---

# FORWARD ROADMAP

## 12. M2 — Raw topology capture + latest-wins worker

**Do not start until M0 direct acceptance is recorded.**

### Goal

Create the structural runtime plumbing while **camera remains pass-through**:

```text
OBS/control thread
→ bounded RawTopologySnapshot
→ single latest-wins worker
→ immutable prepared CanonicalTopology
→ coherent double-buffer publication
```

### Required scope

- max 8 Presentation Screens;
- value-only raw snapshots;
- source UUID carried as durable identity value;
- physical monitor rect;
- source size;
- crop;
- supported scene transform input;
- visibility/eligibility flags;
- monotonic generation;
- one pending/latest request;
- deterministic worker start/stop/join;
- stale generation cannot publish after a newer request;
- no OBS/Qt runtime pointers in worker state;
- no camera or pointer-driven feature behavior yet.

### Required deterministic tests

At minimum:

1. 1,000+ dirty notifications collapse to the latest generation;
2. pending queue depth never grows beyond latest-wins semantics;
3. stale completed generation cannot overwrite newer requested state;
4. worker idle is not busy-spin;
5. shutdown during preparation joins safely;
6. failed candidate does not publish partial/torn state;
7. reader sees coherent old/new snapshot only;
8. screen cap is bounded;
9. no runtime pointer type is embedded in canonical worker data;
10. existing 21+ regression tests remain green.

### M2 stop conditions

Stop if:

- worker needs arbitrary OBS scene APIs off the owning thread;
- publication can tear;
- an unbounded queue appears;
- hot reader requires a long mutex;
- Single changes.

### M2 direct gate

Because camera is still pass-through, direct validation is structural/stability only:

- OBS loads;
- ArZoom Multi remains visually pass-through;
- adding/removing/reordering eligible sources does not crash/hang;
- worker shuts down cleanly on filter removal/OBS exit;
- no log storm.

Do **not** claim cursor mapping correct yet. That is M3.

---

## 13. M3 — ScenePointer diagnostic probe

**Critical gate. Camera must still not move.**

### Goal

Wire real OBS/platform topology into M1 math and expose bounded diagnostics only.

Properties/diagnostic state must prove:

```text
physical cursor
→ Physical Monitor A/B
→ exact Display Capture UUID
→ source UV
→ scene (x,y)
```

### Direct hardware acceptance

Required before M4:

- cursor center/edges on Monitor A map to source A scene region;
- cursor center/edges on Monitor B map to source B scene region;
- two Display Captures scaled smaller and placed side-by-side map correctly;
- physical desktop and OBS canvas coordinates are visibly distinct and still map correctly;
- left/right ordering works;
- negative virtual-desktop coordinates where available;
- mixed DPI where available;
- ambiguous duplicate monitor claims fail safe;
- cursor outside eligible screens pauses mapping;
- no camera movement.

**If M3 is wrong, STOP. Never connect the camera as a workaround.**

---

## 14. M4 — Camera-only Multi

Only after M3 physical proof.

### Goal

Feed the one proven `CanonicalScenePointer` into the **existing**:

- `SceneViewportPlanner`
- `SceneKinematicMotion`
- one scene-level camera

No second planner/camera.

### Direct gate

Repeated A ↔ B transitions must:

- move to the correct scene region;
- remain scene-level;
- be smooth through existing kinematics;
- not reset/snap camera ownership;
- not replay Cinematic Spotlight due merely to screen crossing;
- keep Zoom +/- resize-only;
- avoid boundary chatter/guessing.

No click/cursor/Spotlight Multi wiring yet.

---

## 15. M5 — Shared presentation consumers

### Goal

All presentation features consume the exact same canonical mapping seam:

- Smart Follow/camera;
- click capture/anchoring;
- Presentation Cursor;
- Spotlight Cursor;
- Spotlight Smart Focus;
- Spotlight Click.

Rules:

- live pointer consumers use the current canonical pointer;
- click captures scene/content location at event time;
- historical click remains anchored even if active monitor later changes;
- no feature may independently resolve a Display Capture.

### Direct gate

A/B repeated tests must show click, Classic Hand/Presentation Cursor, and Spotlight land at the same correct location as the camera target.

---

## 16. M6 — Production UX / persistence / Setup Doctor

### Goal

Beginner-facing Presentation Screens UX:

```text
Presentation Screens
☑ Display Capture — Coding
☑ Display Capture — Application
☐ Display Capture — OBS / Utility

Active screen: Automatic from cursor
```

Requirements:

- selection persisted by UUID;
- rename same UUID preserved;
- delete/recreate new UUID does not inherit;
- restart persistence;
- missing/hidden/reconnected states;
- authority conflict diagnostic;
- transform unsupported diagnostic;
- duplicate-monitor ambiguity diagnostic;
- bounded state-based logs;
- no per-frame settings writes.

---

## 17. M7 — Performance, compatibility, direct acceptance closeout

Required before Issue #25 can become stable candidate:

### CI / deterministic

- full P0–P5/P4.1 regression;
- all M0–M6 tests;
- Windows package/installer;
- stable supported OBS lane;
- current next-major OBS lane, including OBS 32.x while current.

### Performance

Verify/measured:

- zero allocation/video tick;
- zero scene enumeration/video tick;
- zero settings write/video tick;
- no blocking render lock;
- no CPU readback;
- no duplicate full-scene render;
- no worker backlog;
- bounded diagnostics;
- 30/60/120/144 FPS stress where practical.

### Direct hardware matrix

- two physical monitors minimum;
- two independently scaled/positioned Display Captures;
- repeated A ↔ B crossing;
- source reorder;
- independent crop/inset;
- utility third Display Capture excluded;
- negative coordinates;
- mixed DPI where available;
- hide/delete/restore;
- monitor disconnect/reconnect;
- OBS restart;
- Toggle Zoom stress;
- Zoom +/- stress;
- click/cursor/Spotlight consistency;
- Intel/AMD/NVIDIA coverage as available;
- no black frame;
- no D3D11 unset-shader warning;
- no Properties flicker.

Only after the project owner explicitly accepts direct M7 evidence should:

- stable docs be updated;
- README/support matrix advertise Multi;
- baseline/release promotion begin.

---

# PROHIBITIONS / STOP CONDITIONS

## 18. Things a future thread must never do

- do not reopen PR #27 runtime architecture;
- do not rename internal ID `arzoom_filter`;
- do not place Multi topology ownership inside Single;
- do not add per-source zoom;
- do not create a second semantic planner/camera;
- do not use persistent `obs_sceneitem_set_*` camera mutation;
- do not duplicate/off-screen-render the scene as a private camera;
- do not add hidden helper scenes/sources;
- do not use CPU frame readback;
- do not use OCR/vision for monitor ownership;
- do not choose nearest/largest/first source as fallback;
- do not persist source name/order/runtime pointer identity;
- do not enumerate scene every video frame;
- do not write settings/files every video frame;
- do not format/log diagnostics every video frame;
- do not create an unbounded queue/history;
- do not detach worker threads;
- do not add sleep/delay as a race fix;
- do not weaken existing stable P0–P5 gates;
- do not touch Issue #26 in this workstream.

## 19. Immediate stop conditions

Stop and root-cause before another patch if:

- physical cursor/display coordinate spaces cannot be proven equivalent;
- source UUID → scene item identity is ambiguous;
- direct M3 probe disagrees with M1 math;
- worker needs lifecycle-sensitive OBS calls off-thread;
- publication can expose partial state;
- a consumer starts resolving its own source independently;
- Single behavior changes;
- black frame appears;
- D3D11 shader warnings regress;
- three consecutive patches in one subsystem still treat symptoms.

---

# HANDOFF OPERATING PROCEDURE

## 20. Completion record format

At the end of every milestone, record:

```text
Milestone completed:
Runtime commit SHA:
Parent SHA:
Files changed:
Architecture/state owner:
Canonical invariant preserved/changed:
Worker/coalescing impact:
Hot-path impact:
Tests executed:
Performance/resource evidence:
CI run / OBS versions:
Direct OBS evidence:
Known limitation/blocker:
Next ONE action:
```

If direct evidence is pending, write **PENDING**. Never call the milestone accepted merely because CI is green.

## 21. Conversation-limit recovery

If a thread is near context/token limit:

1. stop before beginning another milestone;
2. update this ledger with exact current status;
3. include runtime SHA, CI run, tests, blocker, and next ONE action;
4. update PR #34 body if material evidence/architecture changed;
5. push the handoff commit;
6. leave PR #34 Draft unless the full acceptance criteria are met;
7. start a fresh thread;
8. fresh thread follows section 2 mandatory read order;
9. fresh thread re-fetches PR #34 and current head before editing.

## 22. Fresh-thread instruction template

A user can paste this into a new ChatGPT thread:

> Maintain `masarray/arzoom-follow-obs` Issue #25 / ArZoom Multi.  
> First read `AGENTS.md`, `docs/PROJECT_DIRECTION.md`, `docs/P4_1_STABLE_BASELINE.md`, `docs/P5_STABLE_BASELINE.md`, `docs/ARZOOM_MULTI_CANONICAL_IMPLEMENTATION.md`, and `docs/ARZOOM_MULTI_HANDOFF.md`.  
> Then fetch Issue #25 and Draft PR #34 and verify the exact current head.  
> Treat the repository handoff as source of truth; PR #27 is retired failure evidence.  
> Execute only the **Next ONE action** in the handoff, run its exact tests/CI, update the handoff before stopping, and do not touch Issue #26.

---

## 23. Current exact next action

Current engineering state:

- M0 implementation: green in CI, physical boundary acceptance still pending
- M1 pure canonical engine: complete and green
- M2: intentionally blocked

Therefore the next action is exactly:

> **Run and record M0 direct OBS filter-list/pass-through acceptance using Build #287.**

After that passes:

> **M2 — Raw topology capture + latest-wins/coalescing worker**

Nothing from M3/M4/M5/M6/M7 should be bundled into M2.
