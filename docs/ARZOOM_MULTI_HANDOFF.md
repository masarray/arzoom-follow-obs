# ArZoom Multi — Living Handoff Ledger

**Purpose:** make Issue #25 resumable from a fresh thread/agent session without relying on conversation memory.

**Rule:** update this file after every implementation milestone, architecture decision, direct OBS gate, or blocker. If chat memory disagrees with this file, the repository wins.

---

## 1. Current canonical status

- Product tracker: **Issue #25 — Multi-Screen Smart Camera**.
- Canonical branch: `feature/arzoom-multi-canonical`.
- Canonical Draft PR: **#34 — ArZoom Multi: canonical architecture + living implementation handoff**.
- Base branch: `main`.
- Reset base SHA: `ada8f5269246c64429d7aceb6cc72f81e72120ba`.
- Stable public baseline: ArZoom v0.7.0.
- Stable camera/mapping baseline: P4.1.
- Stable presentation baseline: P5.
- Old experimental PR #27: **closed, superseded, do not continue**.
- Old CI-only PR #32: closed; no product scope.
- Issue #26 remains out of scope.

### Milestone state

| Milestone | Status | Evidence |
|---|---|---|
| Strategy reset / canonical docs | COMPLETE | PR #34 docs + Build Windows #284 PASS |
| M0 Dual-filter registration | **IMPLEMENTED / CI PASS / DIRECT OBS TRIAL PENDING** | runtime commit `882053b412f1b2320fe798af313b3f14467c2296`, Build Windows #285 PASS |
| M1 Pure canonical coordinate engine | NOT STARTED | — |
| M2 Topology capture + coalescing worker | NOT STARTED | — |
| M3 ScenePointer diagnostic probe | NOT STARTED | — |
| M4 Camera-only Multi | NOT STARTED | — |
| M5 Shared presentation consumers | NOT STARTED | — |
| M6 UX/persistence/Setup Doctor | NOT STARTED | — |
| M7 Performance/compatibility/acceptance | NOT STARTED | — |

**Next ONE action:** complete the **M0 direct OBS filter-list/pass-through acceptance** described in section 11.

**Do not start M1 until that M0 direct trial passes.**

---

## 2. User outcome / coordinate truth

The real scenario is one OBS scene containing multiple Display Captures that may be independently scaled and positioned:

```text
Physical desktop
Monitor A                       Monitor B
┌──────────────────┐            ┌──────────────────┐
│ real mouse       │            │ real mouse       │
└──────────────────┘            └──────────────────┘
          │                               │
          ▼                               ▼
Display Capture A                 Display Capture B
          │                               │
          └──────────────┬────────────────┘
                         ▼
OBS scene/canvas
┌───────────────────────┬───────────────────────┐
│ A scaled/positioned   │ B scaled/positioned   │
│ independently         │ independently         │
└───────────────────────┴───────────────────────┘
```

The pointer must be mapped in this order:

```text
physical cursor
→ physical display ownership
→ eligible Display Capture UUID
→ source-local UV
→ that source's actual OBS scene transform
→ ONE canonical scene pointer
→ existing planner / kinematics / scene camera
```

Physical-desktop coordinates must never be treated as OBS-canvas coordinates.

---

## 3. Frozen product decisions

### Filter naming and identity

- Stable existing filter user-facing name: **ArZoom Single**.
- Stable existing internal OBS ID: **`arzoom_filter`** — never rename it; old OBS projects persist this ID.
- New filter user-facing name: **ArZoom Multi**.
- New internal OBS ID: **`arzoom_filter_multi`**.

### Single is frozen

ArZoom Single is the golden stable reference. Do not insert Multi ownership, topology worker, or canonical multi-screen state into the Single mapping path.

### One camera authority

One source/scene may have **Single XOR Multi** as camera authority. Multi must fail safe/pass through when the camera-authority boundary is ambiguous; it must not auto-disable user configuration.

### Identity and guessing

Presentation Screens will be persisted by source UUID, never by source display name, scene-item order/index, or raw pointer. Rename same UUID preserves identity; delete/recreate is a new identity.

Ambiguous/missing/unsupported geometry fails safe. Never choose nearest/largest/first source merely to keep the camera moving.

---

## 4. Retired architecture

PR #27 used:

```text
physical cursor
→ physical monitor
→ synthetic/mapped monitor
→ inherited phase1->monitor normalization
→ scene camera
```

It passed deterministic tests and CI but repeatedly failed direct OBS 32.1.2 dual-monitor ownership: Screen 2 did not reliably acquire the pointer. A later monitor-ID patch also failed physical retest.

Therefore:

- the synthetic-monitor P4.2 wrapper stack is retired for Multi;
- this is not a boundary-hysteresis problem;
- useful test/UUID/fail-safe lessons from PR #27 may be reused, but not its runtime architecture.

---

## 5. Canonical runtime architecture

Read `docs/ARZOOM_MULTI_CANONICAL_IMPLEMENTATION.md` before coding.

Target runtime shape:

```text
OBS/control thread
   capture bounded raw topology
           │
           ▼
one latest-wins/coalescing topology worker
   validate + precompute canonical transforms
           │
           ▼
coherent immutable CanonicalTopology snapshot
           │
           ▼
video tick: bounded/no allocation
   cursor → owner → UV → scene
           │
           ▼
CanonicalScenePointer
      ┌────┼────┬─────┐
      ▼    ▼    ▼     ▼
   Camera Click Cursor Spotlight
```

No consumer may independently rediscover its own monitor/source/mapping.

---

## 6. Worker/coalescing/publication decision

This is for **M2**, not M0/M1.

Use one owned topology-preparation worker, not a general job pool. It handles structural validation/precomputation only; cheap per-frame pointer mapping stays on video tick.

Latest-wins contract:

```text
requested generations 101,102,103,104
→ one pending slot
→ obsolete unstarted work disappears
→ stale completed candidate is not published
→ build latest generation
```

No unbounded FIFO backlog. Worker owns no OBS/Qt lifecycle pointers. Shutdown joins deterministically.

Preferred publication is fixed-size double buffering with atomic published index/generation. Readers see a complete old or complete new snapshot and do not wait on a long mutex.

Initial Presentation Screen cap: **8**.

---

## 7. Performance contract

Steady-state target:

```text
heap allocations/video tick     = 0
scene enumeration/video tick    = 0
settings writes/video tick      = 0
file I/O/video tick             = 0
blocking render locks           = 0
CPU frame readback              = 0
extra full-scene render pass    = 0
topology backlog                = 0 (latest-wins)
```

Do not claim optimization without benchmark/resource evidence.

---

## 8. M0 implementation record

**Milestone completed:** M0 — Dual-filter registration boundary (implementation + CI; direct OBS acceptance pending)

**Runtime commit SHA:** `882053b412f1b2320fe798af313b3f14467c2296`

**Commit message:** `feat(multi): establish M0 dual-filter boundary`

**Files changed in the runtime commit:**

- `CMakeLists.txt`
- `src/arzoom-filter-boundary.hpp` — new
- `src/arzoom-filter-multi.cpp` — new
- `src/plugin-main.cpp`
- `tests/CMakeLists.txt`
- `tests/arzoom-m0-filter-boundary-test.cpp` — new

### Architecture / state owner

M0 establishes only the product/runtime registration boundary:

```text
arzoom_filter       → ArZoom Single → existing stable runtime
arzoom_filter_multi → ArZoom Multi  → M0 pass-through shell
```

The stable Single translation unit remains `src/arzoom-filter-v24.cpp`. No PR #27 P4.2 wrapper was restored.

`plugin-main.cpp` overrides only the **display-name callback** for the existing stable source info at module load, returning `ArZoom Single`; the persisted internal ID remains exactly `arzoom_filter`.

### ArZoom Multi M0 behavior

`arzoom_filter_multi` is a distinct OBS filter type but intentionally has:

- no canonical coordinate mapper;
- no Presentation Screen settings;
- no worker;
- no video-tick camera logic;
- no Spotlight/click/cursor integration;
- no dynamic Properties surface;
- no camera movement.

Its render callback is deliberately pass-through via `obs_source_skip_video_filter()`.

### Camera-authority conflict guard

M0 adds a conservative structural guard for sibling camera filter identities. It observes parent filter add/remove events and rescans the parent only when filter topology changes.

Conflict is reported for:

- Single + Multi on the same source;
- duplicate Single authorities;
- duplicate Multi authorities.

M0 does not auto-disable/delete/reorder filters. Multi remains pass-through.

The M0 guard is intentionally **presence-based**, not settings/enable-state aware. More elaborate runtime authority policy is not needed before Multi owns a camera and should not be added speculatively.

### Worker / coalescing impact

**None.** No worker/thread/queue/coalescer exists in M0.

### Hot-path impact

For the new Multi shell:

- no `video_tick` callback;
- render path: one safe pass-through call;
- no per-frame scene/filter enumeration;
- no per-frame settings access/write;
- no per-frame logs;
- conflict enumeration happens only on structural `filter_add` / `filter_remove` events;
- no extra render pass, CPU readback, or scene-item transform mutation.

Stable Single camera/render behavior was not changed.

---

## 9. M0 deterministic/CI evidence

**Build Windows #285**

- exact runtime head: `882053b412f1b2320fe798af313b3f14467c2296`
- OBS compile lane: **OBS Studio 31.1.1 x64**
- MSVC plugin compile: PASS
- `src/arzoom-filter-multi.cpp` compiled into `arzoom.dll`: PASS
- `arzoom.dll`: PASS
- ZIP package: PASS
- Inno Setup installer: PASS
- Windows artifact upload: PASS
- Windows package artifact ID: `10349797091`
- Windows artifact SHA-256: `743bc09fb530e0fc1a2c3f4a527de3e7a5ec571a842d35a4d9b5d78924bee39c`
- benchmark artifact ID: `10350965100`

CTest result: **20/20 PASS, 0 failed**.

New M0 gate:

- `arzoom-m0-dual-filter-boundary` — PASS

It locks:

- legacy Single internal ID remains exactly `arzoom_filter`;
- Scene Camera stable ID still equals the legacy Single ID;
- user-facing names are exactly `ArZoom Single` and `ArZoom Multi`;
- Multi internal ID is exactly `arzoom_filter_multi` and distinct;
- foreign/null IDs do not classify as a camera authority;
- Single-only and Multi-only are non-conflicting;
- Single+Multi and duplicate authority identities are conflicts.

All existing P0–P5/P4.1 deterministic gates also remain green.

### Performance evidence

No performance improvement is claimed for M0. It introduces no Multi steady-state video-tick work and no worker. Existing benchmark suite completed successfully in Build #285; absolute hosted-runner timings are diagnostic only.

---

## 10. M0 direct OBS evidence

**PENDING.** CI cannot prove OBS filter-list UX, loading of an existing saved project, or real pass-through behavior in the user's installed OBS build.

Do **not** label M0 fully accepted and do **not** start M1 until the following small direct trial passes.

---

## 11. Next ONE action — M0 direct filter-list/pass-through trial

Use the Build Windows #285 artifact. This is **not** a multi-monitor test yet.

Required observations:

1. Install the M0 build and launch the user's current OBS.
2. Open an existing scene/project that already contains the old ArZoom filter.
   - It must load normally because internal ID `arzoom_filter` was preserved.
   - In the Add Filter/type UI it is now presented as **ArZoom Single**.
   - Existing Single behavior must remain unchanged.
3. Open **Add Filter**.
   - **ArZoom Single** must be listed.
   - **ArZoom Multi** must be listed separately.
4. Add **ArZoom Multi** alone to a suitable video/scene source.
   - Video must remain visually unchanged/pass-through.
   - No Multi camera/follow behavior is expected in M0.
5. Put Single and Multi on the same source for the conflict test.
   - OBS must remain stable.
   - Log should contain one transition warning beginning `[ArZoom Multi] Camera authority conflict...`, not per-frame spam.
6. Remove the conflicting Single or Multi filter.
   - Conflict-cleared transition should be logged once.
7. Confirm no black frame/crash/Properties flicker is introduced by this boundary build.

If any item fails, stop on M0 and fix the exact boundary defect. Do not move to canonical coordinate work.

If all items pass, update this ledger to **M0 ACCEPTED** and set the next ONE milestone to:

> **M1 — Pure canonical coordinate engine**

M1 remains pure math/state only; no worker and no runtime camera wiring.

---

## 12. Required milestone order

### M0 — Dual-filter registration boundary
Current state: implementation/CI PASS, direct OBS trial pending.

### M1 — Pure canonical coordinate engine
Only after M0 direct acceptance. Must mathematically prove two independently scaled/positioned Display Captures map `physical → source UV → scene` correctly. No runtime camera.

### M2 — Raw topology capture + coalescing worker
Bounded raw snapshot, latest-wins worker, coherent publication, deterministic lifecycle tests.

### M3 — Diagnostic ScenePointer probe
**Critical physical gate. Camera still must not move.** Direct OBS must visibly prove:

```text
Physical Monitor A/B
→ correct Display Capture UUID
→ correct source UV
→ correct scene coordinate
```

Do not implement M4 before M3 passes on real hardware.

### M4 — Camera-only Multi
Feed the proven canonical scene pointer into the existing planner/kinematics.

### M5 — Shared click/cursor/Spotlight consumers
All consume the same canonical pointer/event seam. No independent resolver.

### M6 — Production UX/persistence/Setup Doctor
UUID selection, restart persistence, lifecycle/failure states, beginner wording.

### M7 — Performance + compatibility + direct acceptance
Supported stable OBS lane plus current next-major including OBS 32.x; mixed DPI, negative desktop coordinates, reconnect/restart, FPS stress, and GPU coverage as available.

---

## 13. Mandatory read order for a fresh thread

1. `AGENTS.md`
2. `docs/PROJECT_DIRECTION.md`
3. `docs/P4_1_STABLE_BASELINE.md`
4. `docs/P5_STABLE_BASELINE.md`
5. `docs/ARZOOM_MULTI_CANONICAL_IMPLEMENTATION.md`
6. **this file**
7. Issue #25
8. Draft PR #34

Then verify current PR #34 head before editing. Do not trust a SHA copied from an old conversation.

Do not start by reading/continuing PR #27 as current architecture.

---

## 14. Per-milestone completion record

At the end of each milestone update this ledger with:

```text
Milestone completed:
Runtime commit SHA:
Files changed:
Architecture/state owner:
Canonical invariant preserved/changed:
Worker/coalescing impact:
Hot-path impact:
Tests executed:
Performance evidence:
CI / OBS versions:
Direct OBS evidence:
Known limitation/blocker:
Next ONE milestone/action:
```

If direct OBS evidence is pending, write `PENDING`; do not call the milestone accepted.

---

## 15. Conversation-limit recovery

If a thread approaches its context/token limit:

1. stop before beginning a new milestone;
2. update this ledger with exact status, runtime SHA, tests, CI, direct evidence, blocker, and next ONE action;
3. update Draft PR #34 if architecture/evidence changed materially;
4. commit/push the handoff;
5. start a fresh thread;
6. the new thread follows the mandatory read order and verifies branch/PR head before editing.

Never rely on “the previous assistant remembers it.”

---

## 16. Future-thread prohibitions

- do not merge/reopen PR #27 architecture;
- do not rename internal ID `arzoom_filter`;
- do not put Multi ownership logic inside Single;
- do not implement M1 before M0 direct acceptance;
- do not implement M4 before M3 physical mapping proof;
- do not add per-source zoom;
- do not add a second planner/camera authority;
- do not add CPU readback/OCR/vision;
- do not use persistent scene-item transform mutation as the camera mechanism;
- do not add per-frame scene enumeration/settings writes/log formatting;
- do not add an unbounded worker queue;
- do not weaken stable P0–P5 tests;
- do not touch Issue #26 while Issue #25 work is scoped here.
