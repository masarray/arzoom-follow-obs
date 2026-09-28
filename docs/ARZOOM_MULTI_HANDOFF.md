# ArZoom Multi — Living Handoff Ledger

**Purpose:** make Issue #25 resumable from a fresh ChatGPT/thread/agent session without relying on hidden conversation history.

**Authority rule:** for Issue #25, this file is the execution ledger and `docs/ARZOOM_MULTI_CANONICAL_IMPLEMENTATION.md` is the architecture source of truth. If chat memory disagrees with the repository, the repository wins.

**Scope rule:** Issue #25 only. Issue #26 remains out of scope.

---

## 1. Current canonical status

- Repository: `masarray/arzoom-follow-obs`
- Tracker: **Issue #25 — Multi-Screen Smart Camera**
- Canonical branch: `feature/arzoom-multi-canonical`
- Canonical Draft PR: **#34**
- Base branch: `main`
- Reset base SHA: `ada8f5269246c64429d7aceb6cc72f81e72120ba`
- Stable public baseline: **ArZoom v0.7.0**
- Stable camera/mapping baseline: **P4.1**
- Stable presentation baseline: **P5**
- Retired failed experiment: **PR #27 — do not resume**
- Old CI-only PR #32: closed, no product scope
- Latest implementation SHA: **`b4155f4ecfc4178ebe5443a7020ae9d6adb23388`**
- Latest implementation message: **`feat(multi): add M2A latest-wins topology core`**
- M1 and M2A are pure infrastructure only; ArZoom Multi runtime is still the M0 pass-through shell.
- This handoff file may be committed after the implementation SHA. A fresh thread must always re-fetch PR #34 and verify its actual current head before editing.

### Milestone state

| Milestone | Status | Evidence |
|---|---|---|
| Strategy reset / canonical docs | COMPLETE | PR #34; Build #284 PASS |
| M0 Dual-filter registration boundary | **IMPLEMENTED / CI PASS / DIRECT OBS TRIAL PENDING** | `882053b412f1b2320fe798af313b3f14467c2296`; Build #285 PASS |
| M1 Pure canonical coordinate engine | **COMPLETE / CI PASS / PURE ONLY** | `750600607c920078a75f8fbe2e37db3b54ec7cb6`; Build #287; 21/21 PASS |
| M2A Pure topology preparation + latest-wins worker/publication core | **COMPLETE / CI PASS / NO OBS WIRING** | `b4155f4ecfc4178ebe5443a7020ae9d6adb23388`; Build #289; 22/22 PASS |
| M2B OBS raw-topology capture + worker integration | **BLOCKED / NOT STARTED** | blocked by pending M0 direct OBS boundary acceptance |
| M3 ScenePointer diagnostic probe | NOT STARTED | camera must still not move |
| M4 Camera-only Multi | NOT STARTED | blocked until M3 physical proof |
| M5 Shared click/cursor/Spotlight consumers | NOT STARTED | — |
| M6 UX/persistence/Setup Doctor | NOT STARTED | — |
| M7 Performance/compatibility/direct acceptance | NOT STARTED | — |

### Next ONE action

> **Complete the M0 direct OBS filter-list/pass-through acceptance using Build #289 or any later docs-only package from the same runtime.**

M1 and M2A are not wired into the OBS filter, so the runtime behavior under test is still exactly the M0 pass-through boundary.

**Do not begin M2B runtime integration until this M0 direct boundary gate is recorded PASS.**

---

## 2. Mandatory read order for every fresh thread

Before implementation, read in this exact order:

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
3. inspect CI for that exact head;
4. execute only the **Next ONE action/milestone** recorded here;
5. update this file before stopping.

Never rely on “the previous assistant remembers it.”

---

## 3. User outcome / coordinate truth

The actual user scenario is one OBS scene containing multiple independently scaled/positioned Display Captures:

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

Canonical mapping order:

```text
physical cursor
→ deterministic physical display ownership
→ eligible Display Capture UUID
→ source-local/full-source UV
→ crop-visible domain
→ that exact source's prepared source→scene transform
→ ONE CanonicalScenePointer
→ existing SceneViewportPlanner
→ existing SceneKinematicMotion
→ ONE scene-level camera
```

Physical Windows desktop coordinates are **not** OBS canvas coordinates.

---

## 4. Frozen decisions

### Filter identity

- Existing stable display name: **ArZoom Single**
- Existing internal OBS ID: **`arzoom_filter`**
- New filter display name: **ArZoom Multi**
- New internal OBS ID: **`arzoom_filter_multi`**

Never rename the legacy internal ID; existing OBS projects persist it.

### Single is frozen

ArZoom Single is the golden P4.1/P5 reference. Do not insert Multi ownership, topology worker, or canonical Multi state into the Single mapping path.

### One camera authority

A source/scene may have **Single XOR Multi** as camera authority. Multi fails safe/pass-through when authority is ambiguous and never auto-disables/deletes/reorders the user's filters.

### Durable identity

Presentation Screens are persisted by **source UUID**, never by:

- display name;
- scene-item order/index;
- raw runtime pointer;
- “first/nearest/largest” source.

Rename same UUID preserves identity. Delete/recreate with a new UUID is a new identity.

### No guessing

Missing/ambiguous/unsupported geometry pauses pointer retargeting. Never approximate merely to keep the camera moving.

---

## 5. Retired architecture — PR #27

Retired path:

```text
physical cursor
→ physical monitor
→ synthetic/mapped monitor
→ inherited phase1->monitor normalization
→ scene camera
```

Direct OBS 32.1.2 testing repeatedly failed Screen 2 ownership. A later monitor-ID patch also failed.

Therefore:

- PR #27 is failure evidence/test-idea history only;
- do not reopen the synthetic-monitor wrapper stack;
- do not patch that ownership model again;
- Multi is source-first: **physical → source local → scene**.

---

## 6. Canonical target architecture

```text
OBS/control thread
   capture bounded RawTopologySnapshot
               │
               ▼
ONE topology preparation worker
latest-wins / coalescing / no FIFO backlog
               │
               ▼
validate + precompute
               │
               ▼
immutable CanonicalTopology
coherent fixed-slot publication
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

No consumer may rediscover its own monitor/source/mapping independently.

---

# IMPLEMENTATION RECORD

## 7. M0 — Dual-filter registration boundary

### Commit

`882053b412f1b2320fe798af313b3f14467c2296`

`feat(multi): establish M0 dual-filter boundary`

### Runtime boundary

```text
arzoom_filter
└─ ArZoom Single
   └─ stable existing runtime

arzoom_filter_multi
└─ ArZoom Multi
   └─ deliberate pass-through shell
```

M0 Multi has no mapper, worker, camera, Presentation Screen persistence, click/cursor/Spotlight wiring, or dynamic Properties. Render is pass-through via `obs_source_skip_video_filter()`.

Conflict detection is structural/event-driven on parent filter add/remove. It does not poll per frame and does not mutate user configuration.

### M0 CI

Build Windows **#285**:

- 20/20 CTest PASS
- `arzoom-m0-dual-filter-boundary` PASS
- all stable P0–P5/P4.1 gates PASS
- OBS 31.1.1 x64 compile PASS
- DLL/ZIP/installer PASS

### M0 direct evidence

**PENDING.**

Do not call M0 accepted until a real OBS trial proves legacy load, filter-list naming, pass-through behavior, conflict stability, and no black frame/crash/flicker.

---

## 8. M1 — Pure canonical coordinate engine

### Commit

`750600607c920078a75f8fbe2e37db3b54ec7cb6`

`feat(multi): add M1 canonical coordinate engine`

### Files

- `src/arzoom-multi-coordinate.hpp`
- `tests/arzoom-m1-coordinate-engine-test.cpp`
- `tests/CMakeLists.txt`
- `CMakeLists.txt`

### Contract

M1 defines bounded value-only coordinate state:

- max 8 Presentation Screens;
- signed 64-bit physical desktop rectangles;
- half-open ownership: `left <= x < right`, `top <= y < bottom`;
- overflow-safe normalization using wide intermediate arithmetic;
- source UV/full-source pixel mapping;
- crop-visible-domain validation;
- general affine representation;
- v1 accepts finite positive axis-aligned translation + independent X/Y scale;
- rotation/skew/flips/degenerate geometry fail safe;
- ambiguity/outside/no-eligible/too-many-screen states;
- no nearest/largest/first fallback.

M1 is **not called by OBS runtime**.

### M1 deterministic evidence

Tests prove:

- two independently scaled/positioned side-by-side captures;
- unequal source sizes;
- negative desktop coordinates;
- exact adjacent-monitor boundary ownership;
- outside/ambiguity/no-eligible behavior;
- unchecked utility monitor exclusion;
- crop mapping and cropped-away rejection;
- invalid/exhaustive crop rejection;
- rotation/skew/flip rejection;
- invalid source/physical geometry fail-safe;
- max-screen cap;
- extreme signed-desktop normalization without overflow.

Build Windows **#287**:

- 21/21 CTest PASS
- M1 gate PASS
- OBS 31.1.1 compile/package PASS
- artifact ID `10958317003`
- digest `sha256:8b85618888c55096df2e2538be63ea73e992572a4a8c9f8bcc0e6fc12d817e00`

### Runtime/hot-path impact

Zero. M1 is header-only pure infrastructure and is not invoked by Single or Multi runtime.

---

## 9. Controlled gate deviation

The original ledger blocked all post-M0 work until the M0 direct trial.

The project owner explicitly instructed continued progress before supplying that trial. A controlled deviation was therefore allowed only for **non-runtime pure infrastructure**:

- M1 pure coordinate engine was allowed;
- later, after another explicit continue instruction, M2A pure worker/publication core was allowed;
- neither is instantiated by `arzoom_filter_multi`;
- M0 remains unaccepted;
- **M2B OBS integration remains blocked**.

A future thread must not interpret M1/M2A CI success as proof that M0 passed direct OBS acceptance.

---

## 10. M2A — Pure topology preparation + latest-wins worker/publication core

### Commit

`b4155f4ecfc4178ebe5443a7020ae9d6adb23388`

`feat(multi): add M2A latest-wins topology core`

Parent: `1e5a700650946f9df910b22938c32938531c376e`

### Files

1. `src/arzoom-multi-topology.hpp`
2. `tests/arzoom-m2-topology-worker-test.cpp`
3. `tests/CMakeLists.txt`
4. `CMakeLists.txt`

**No OBS runtime source calls M2A.**

### Value-only topology

M2A adds fixed-size, trivially-copyable state:

- `MultiSourceUuid` — fixed 64-byte UUID value storage;
- `MultiRawPresentationScreen`;
- `MultiRawTopologySnapshot`;
- `MultiCanonicalScreen`;
- `MultiCanonicalTopology`;
- fixed arrays capped by M1's 8-screen maximum.

No OBS pointer, scene-item pointer, Qt object, vector, shared_ptr, or unbounded container is stored in raw/canonical topology state.

### Pure preparation

`multi_prepare_topology()`:

- carries UUID values;
- preserves generation/count;
- marks unselected screens Ineligible;
- marks hidden screens Hidden;
- rejects missing UUID as InvalidIdentity;
- precomputes M1 source→scene axis-aligned transform;
- rejects invalid geometry;
- enables mapping only for canonical Ready screens;
- returns a current fail-safe topology status for invalid latest topology.

### Latest-wins worker

`MultiTopologyWorker` owns:

- one worker thread;
- one active preparation;
- one fixed pending snapshot slot;
- one monotonic highest-requested generation;
- condition-variable waiting;
- no FIFO queue.

Deterministic burst test:

```text
generation 1 active and blocked
generations 2..1001 requested

result:
build 1    → stale → discard
pending    → only generation 1001 remains
build 1001 → publish
```

For 1,001 accepted requests the gate proves:

- `builds_started == 2`
- `requests_coalesced == 999`
- `stale_builds_discarded == 1`
- `builds_published == 1`
- final published generation = `1001`

Older/out-of-order generations are ignored.

### Coherent publication

`MultiTopologyPublisher` uses two fixed canonical slots.

Hot reader:

1. loads published slot index;
2. pins that slot with an atomic reader counter;
3. rechecks index;
4. copies one complete fixed-size snapshot;
5. releases pin.

Single writer:

1. chooses inactive slot;
2. waits only for old readers of that inactive slot to drain;
3. writes complete snapshot;
4. publishes slot with a release store.

Properties:

- no reader mutex;
- no heap ownership transfer;
- writer never modifies a pinned reader slot;
- reader sees complete old or complete new topology, never torn state.

A concurrent **5,000-generation** publication stress test passed without observing a torn snapshot.

### Lifecycle

Worker idles on `std::condition_variable`, not a permanent poll loop.

Shutdown:

- marks stopping;
- clears pending;
- wakes worker;
- joins owned thread;
- a build still in preparation is discarded after release;
- no detached thread.

A deterministic shutdown gate blocks generation 1 inside preparation, calls stop from another thread, proves stop does not return early, releases preparation, and verifies join + zero publication.

### M2A preflight

Contract-equivalent local compile:

```text
g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -pthread
PASS
```

### M2A CI

Build Windows **#289** on exact SHA `b4155f4ecfc4178ebe5443a7020ae9d6adb23388`: **PASS**

- 22/22 CTest PASS
- `arzoom-m2a-latest-wins-topology-worker` PASS
- all P0–P5/P4.1/M0/M1 gates PASS
- OBS Studio 31.1.1 x64 compile PASS
- `arzoom.dll` PASS
- ZIP PASS
- Inno Setup installer PASS
- Windows artifact ID: `10960108664`
- digest: `sha256:3060d912850c8f264340126ff6b9e6bcf631185cf5f7beaabf9dd7e890912885`
- benchmark artifact ID: `10960811751`

### Runtime/hot-path impact

**Zero OBS runtime impact.**

M2A exists only as pure infrastructure/tests. ArZoom Multi still has:

- no video tick;
- no topology capture;
- no worker instance;
- no scene enumeration;
- no cursor mapping;
- no camera movement;
- no settings write;
- no render change beyond M0 pass-through.

---

# CURRENT BLOCKER / NEXT ACTION

## 11. M0 direct filter-list/pass-through acceptance

Use Build #289 (or a later docs-only build with the same runtime).

Required observations:

1. Existing project containing legacy ArZoom loads normally as **ArZoom Single**.
2. Existing Single behavior remains unchanged.
3. **Filters → +** lists **ArZoom Single** and **ArZoom Multi** separately.
4. ArZoom Multi alone is visually unchanged/pass-through.
5. Single + Multi on one source remains stable.
6. Conflict log occurs on state transition only, not per frame.
7. Removing conflict produces one cleared transition.
8. No crash.
9. No black frame.
10. No Properties flicker.

If any item fails:

> **STOP on M0 and fix only that boundary defect. Do not wire M2A into OBS.**

If all pass:

1. update M0 to **ACCEPTED**;
2. record actual OBS version/evidence;
3. set Next ONE milestone to **M2B**;
4. begin M2B only after the acceptance update is pushed.

---

# FORWARD ROADMAP

## 12. M2B — OBS raw topology capture + M2A integration

**Blocked until M0 direct acceptance.**

Goal while camera remains pass-through:

```text
OBS/control thread
→ bounded RawTopologySnapshot
→ existing M2A latest-wins worker
→ immutable CanonicalTopology
→ existing M2A coherent publication
```

Required scope:

- capture source UUID as durable value;
- capture physical monitor rectangle in one proven coordinate space;
- source width/height;
- crop;
- supported scene geometry;
- visible/eligible flags;
- monotonic topology generation;
- event-driven dirty notifications;
- start/stop worker with Multi lifecycle;
- no worker-owned OBS/Qt pointers;
- no camera or pointer-driven presentation behavior yet.

Required deterministic/runtime gates:

- structural events coalesce;
- stale generation cannot publish;
- filter removal/OBS exit joins worker;
- no queue growth;
- no per-frame scene enumeration;
- Multi remains pass-through;
- no log storm.

Do not claim physical cursor mapping correct in M2B. That belongs to M3.

---

## 13. M3 — ScenePointer diagnostic probe

**Critical physical gate. Camera still must not move.**

Wire real platform/OBS topology into M1 math and expose bounded diagnostic state proving:

```text
Physical Monitor A/B
→ exact Display Capture UUID
→ source UV
→ scene (x,y)
```

Direct acceptance before M4:

- center/edges Monitor A map to source A;
- center/edges Monitor B map to source B;
- two Display Captures may be scaled smaller and placed side-by-side;
- physical desktop coordinates must remain distinct from scene coordinates;
- left/right/negative coordinates;
- mixed DPI where available;
- duplicate monitor claims fail safe;
- cursor outside eligible displays pauses mapping;
- **no camera movement**.

If M3 is wrong, stop. Never connect camera as a workaround.

---

## 14. M4 — Camera-only Multi

Only after M3 physical proof.

Feed the one proven CanonicalScenePointer into the existing:

- `SceneViewportPlanner`
- `SceneKinematicMotion`
- one scene-level camera

No second planner/camera.

Direct gate:

- repeated A↔B follows correct scene regions;
- smooth kinematic travel;
- no camera snap/reset;
- no cinematic replay merely due to crossing;
- Zoom +/- remains resize-only;
- no guessing/boundary chatter.

No click/cursor/Spotlight Multi wiring yet.

---

## 15. M5 — Shared presentation consumers

All pointer-driven presentation features consume the same canonical seam:

- camera/Smart Follow;
- click capture;
- Presentation Cursor;
- Spotlight Cursor;
- Spotlight Smart Focus;
- Spotlight Click.

Historical click captures its scene location at event time and remains anchored after later monitor switches.

No feature may independently resolve a Display Capture.

---

## 16. M6 — Production UX / persistence / Setup Doctor

Beginner-facing target:

```text
Presentation Screens
☑ Display Capture — Coding
☑ Display Capture — Application
☐ Display Capture — OBS / Utility

Active screen: Automatic from cursor
```

Requirements:

- UUID persistence;
- rename same UUID preserved;
- delete/recreate new UUID does not inherit;
- restart persistence;
- missing/hidden/reconnected diagnostics;
- authority conflict diagnostic;
- unsupported transform diagnostic;
- duplicate-monitor ambiguity diagnostic;
- bounded state-based logs;
- no per-frame settings writes.

---

## 17. M7 — Performance / compatibility / acceptance closeout

Before Issue #25 can become stable candidate:

### CI

- all P0–P5/P4.1 regressions;
- all M0–M6 deterministic gates;
- Windows package/installer;
- supported stable OBS lane;
- current next-major OBS lane, including OBS 32.x while current.

### Performance

Measure/verify:

- zero allocation/video tick;
- zero scene enumeration/video tick;
- zero settings write/video tick;
- no blocking render lock;
- no CPU readback;
- no duplicate full-scene render;
- no topology backlog;
- bounded diagnostics;
- 30/60/120/144 FPS stress where practical.

### Direct hardware

- minimum two physical monitors;
- two independently scaled/positioned Display Captures;
- repeated A↔B;
- source reorder;
- independent crop/inset;
- unselected utility display;
- negative desktop coordinates;
- mixed DPI where available;
- hide/delete/restore;
- monitor disconnect/reconnect;
- OBS restart;
- Toggle Zoom and Zoom +/- stress;
- click/cursor/Spotlight consistency;
- Intel/AMD/NVIDIA coverage as available;
- no black frame;
- no D3D11 unset-shader warning;
- no Properties flicker.

Only after explicit owner acceptance should stable docs/release promotion happen.

---

# PERFORMANCE / SAFETY CONTRACT

## 18. Hot-path targets

```text
heap allocation / video tick      = 0
scene enumeration / video tick    = 0
settings/file writes / video tick = 0
blocking render locks             = 0
CPU frame readback                = 0
extra full-scene render pass      = 0
topology FIFO backlog             = 0
Presentation Screens              <= 8 initially
diagnostic state                  = bounded
idle worker CPU                   ≈ 0
```

Do not claim an optimization successful without benchmark/resource evidence.

---

## 19. Prohibited implementation shortcuts

Never introduce:

- per-source zoom engines;
- a second semantic SceneViewportPlanner;
- persistent `obs_sceneitem_set_*` camera mutation;
- duplicate/off-screen scene rendering;
- hidden helper scene/source;
- CPU frame readback;
- OCR/vision ownership;
- nearest/largest/first source guessing;
- source-name/order/runtime-pointer persistence;
- per-frame settings writes;
- per-frame scene enumeration;
- per-frame diagnostic formatting/log spam;
- unbounded queues/histories;
- detached worker threads;
- arbitrary sleep/delay race fixes;
- weakened P0–P5 regression gates;
- Issue #26 work inside this branch.

---

## 20. Immediate stop conditions

Stop and root-cause if:

- cursor/display coordinate spaces cannot be proven equal;
- source UUID→scene item identity is ambiguous;
- direct M3 evidence disagrees with M1 math;
- worker needs lifecycle-sensitive OBS calls off-thread;
- publication can expose torn/partial state;
- a consumer begins resolving its own source independently;
- Single behavior changes;
- black frames appear;
- D3D11 shader warnings regress;
- three consecutive patches still treat symptoms in the same subsystem.

---

# HANDOFF OPERATING PROCEDURE

## 21. Per-milestone completion record

Always record:

```text
Milestone completed:
Implementation SHA:
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

If direct evidence is pending, write **PENDING**. Never call a runtime milestone accepted solely because CI is green.

---

## 22. Conversation-limit recovery

If a thread approaches its context limit:

1. stop before another milestone;
2. update this file;
3. include exact implementation SHA, CI, tests, blocker, and next ONE action;
4. update Draft PR #34 if material status changed;
5. push the handoff;
6. keep PR Draft until final acceptance;
7. start a fresh thread;
8. fresh thread follows section 2 and re-fetches current PR head.

---

## 23. Fresh-thread prompt template

> Maintain `masarray/arzoom-follow-obs` Issue #25 / ArZoom Multi.  
> First read `AGENTS.md`, `docs/PROJECT_DIRECTION.md`, `docs/P4_1_STABLE_BASELINE.md`, `docs/P5_STABLE_BASELINE.md`, `docs/ARZOOM_MULTI_CANONICAL_IMPLEMENTATION.md`, and `docs/ARZOOM_MULTI_HANDOFF.md`.  
> Then fetch Issue #25 and Draft PR #34 and verify its exact current head.  
> Treat the repository handoff as source of truth; PR #27 is retired failure evidence.  
> Execute only the **Next ONE action** in the handoff, run its tests/CI, update the handoff before stopping, and do not touch Issue #26.

---

## 24. Current exact next action

Current state:

- M0 implementation: CI green, direct OBS boundary acceptance pending;
- M1 pure canonical coordinate engine: complete/green;
- M2A pure worker/publication core: complete/green;
- M2B OBS integration: blocked.

Therefore:

> **Run and record M0 direct OBS filter-list/pass-through acceptance using Build #289 (or a later docs-only package with identical runtime).**

After that passes:

> **M2B — OBS raw topology capture + M2A worker integration**

Do not bundle M3/M4/M5/M6/M7 into M2B.
