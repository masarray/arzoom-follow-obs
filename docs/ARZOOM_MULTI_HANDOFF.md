# ArZoom Multi — Living Handoff Ledger

**Purpose:** this file makes Issue #25 resumable from a fresh ChatGPT/thread/agent session without relying on hidden conversation history.

**Rule:** update this file at the end of every accepted milestone or architecture decision. If this file and chat memory disagree, the repository wins.

---

## 1. Current canonical status

- Product tracker: **Issue #25 — Multi-Screen Smart Camera**.
- Current canonical branch: `feature/arzoom-multi-canonical`.
- Current canonical Draft PR: **#34 — ArZoom Multi: canonical architecture + living implementation handoff**.
- Base branch: `main`.
- Base SHA used for reset: `ada8f5269246c64429d7aceb6cc72f81e72120ba`.
- Stable public baseline: ArZoom v0.7.0.
- Stable camera/mapping baseline: P4.1.
- Stable presentation baseline: P5.
- Old experimental PR #27: **closed, superseded, do not continue**.
- Old CI-only PR #32: already closed; no product scope.
- Issue #26 remains out of scope.

### Current milestone state

| Milestone | Status | Evidence |
|---|---|---|
| Strategy reset / canonical docs | COMPLETE | PR #34 docs committed |
| M0 Dual-filter registration | NEXT / NOT STARTED | — |
| M1 Pure canonical coordinate engine | NOT STARTED | — |
| M2 Topology capture + coalescing worker | NOT STARTED | — |
| M3 ScenePointer diagnostic probe | NOT STARTED | — |
| M4 Camera-only Multi | NOT STARTED | — |
| M5 Shared presentation consumers | NOT STARTED | — |
| M6 UX/persistence/Setup Doctor | NOT STARTED | — |
| M7 performance/compatibility/acceptance | NOT STARTED | — |

**Next ONE milestone:** `M0 — Dual-filter registration boundary`.

Do not start M1 in the same implementation step as M0.

Before editing, verify the current head SHA of PR #34. Do not trust a SHA copied from an older conversation.

---

## 2. What the user actually needs

The important scenario is not merely “two Windows monitors.” It is:

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

The Windows pointer must first be mapped to the **correct physical display/source**, then to **source-local UV**, then through that source's **actual OBS scene transform**.

The pointer must not be treated as if physical-desktop coordinates are already canvas coordinates.

---

## 3. Frozen product decisions

These decisions are already made. Do not reopen them casually.

### Naming

- existing filter display name becomes **ArZoom Single**;
- existing internal ID remains `arzoom_filter` for OBS project compatibility;
- new filter display name is **ArZoom Multi**;
- new internal ID is `arzoom_filter_multi`.

### Single is frozen

ArZoom Single is the golden stable reference. Multi implementation must not be inserted into the existing Single mapping path.

### One camera authority

One scene may have Single **or** Multi active, never both as competing camera authorities.

### Canonical Multi mapping

```text
physical cursor
→ physical display ownership
→ eligible Display Capture UUID
→ source-local UV
→ source's scene transform
→ one canonical scene pointer
→ existing planner/kinematics/camera
```

### Identity

Persist Presentation Screens by **source UUID**, never by:
- source display name;
- scene-item index/order;
- raw runtime pointer.

Rename same UUID preserves identity. Delete/recreate with new UUID is a new identity.

### No guessing

Ambiguous/missing/unsupported geometry fails safe. Never choose nearest/largest/first source merely to keep camera moving.

---

## 4. Why PR #27 failed and is retired

The old P4.2 experiment used this conceptual bridge:

```text
physical cursor
→ physical monitor
→ synthetic/mapped monitor
→ inherited phase1->monitor normalization
→ scene camera
```

It accumulated good pure tests and passed CI, but direct OBS 32.1.2 trials failed the core requirement: the cursor effectively remained associated with Screen 1 and Screen 2 did not reliably acquire ownership.

A later patch mirrored OBS Windows `monitor_id` resolution and tightened fail-safe selection, but physical retest still failed.

Conclusion:

- this was not merely a boundary-hysteresis bug;
- more patches to synthetic-monitor ownership are not justified;
- the Multi architecture must be source-first: **physical → source-local → scene**.

PR #27 is retained only as failure evidence and a source of test ideas.

---

## 5. Architecture source of truth

Read `docs/ARZOOM_MULTI_CANONICAL_IMPLEMENTATION.md` before coding.

Core runtime shape:

```text
OBS/control thread
   capture bounded raw topology
           │
           ▼
latest-wins/coalescing topology worker
   validate + prepare canonical transforms
           │
           ▼
coherent immutable CanonicalTopology snapshot
           │
           ▼
video tick: O(1)/bounded
   cursor → owner → UV → scene
           │
           ▼
CanonicalScenePointer
      ┌────┼────┬─────┐
      ▼    ▼    ▼     ▼
   Camera Click Cursor Spotlight
```

No consumer resolves its own monitor/source independently.

---

## 6. Worker/coalescing decision

Use **one owned topology-preparation worker**, not a general job pool.

Purpose:
- remove structural validation/precomputation from realtime callbacks;
- coalesce bursty source/scene/display changes;
- publish immutable prepared state.

Do not use worker for cheap per-frame pointer mapping.

Latest-wins contract:

```text
requested generation: 101,102,103,104
worker may prepare 101
if 104 is latest before publish/next cycle,
skip obsolete queued intermediates and prepare latest
```

No unbounded FIFO backlog.

Worker owns no OBS/Qt lifecycle pointers.

Shutdown joins worker deterministically.

---

## 7. Publication decision

Hot readers must not wait on a long mutex.

Preferred approach:
- fixed-size double-buffered canonical snapshot;
- one writer;
- atomic published index/generation;
- readers see either complete old snapshot or complete new snapshot;
- zero heap ownership transfer in video/render hot paths.

A custom seqlock is acceptable only if clearly simpler/proven. Prefer double buffer if both solve the requirement.

---

## 8. Performance contract

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

Initial Presentation Screen cap: **8**.

Do not claim performance improvements without benchmark evidence.

---

## 9. Required implementation order

### M0 — Dual-filter registration boundary

Only:
- rename user-facing old filter to `ArZoom Single` while preserving `arzoom_filter` internal ID;
- register new pass-through `arzoom_filter_multi` as `ArZoom Multi`;
- add explicit conflict detection;
- prove old scene compatibility;
- no mapping worker yet.

Stop after M0 tests/direct filter-list trial.

### M1 — Pure canonical coordinate engine

Only pure math/state.

Must prove two independently scaled/positioned sources map correctly.

No OBS runtime camera wiring.

### M2 — Topology capture + coalescing worker

Bounded raw snapshot, latest-wins worker, coherent publication, lifecycle tests.

### M3 — Diagnostic ScenePointer probe

**Critical gate. Camera must still not move.**

Properties/diagnostics must show:

```text
Physical Monitor 2
→ Display Capture 2
→ source UV
→ scene coordinate
```

Direct hardware proof is mandatory before M4.

### M4 — Camera-only Multi

Only after M3 passes physical A/B mapping.

Feed canonical scene pointer into the existing planner/kinematics.

### M5 — Click/Cursor/Spotlight shared consumer wiring

All use the same canonical pointer/event seam.

### M6 — Production UX/persistence/Setup Doctor

UUID selection, restart persistence, lifecycle/failure states, beginner wording.

### M7 — performance + compatibility + direct acceptance

OBS supported stable lane + current next-major lane including OBS 32.x, FPS stress, mixed DPI, negative coordinates, reconnect/restart, GPU coverage as available.

---

## 10. Direct acceptance philosophy

CI can prove math, compile, packaging, and deterministic contracts.

CI cannot prove actual physical-monitor ownership or visual correctness on the user's two-monitor machine.

Every runtime milestone has a direct OBS gate. A milestone is not accepted merely because CI is green.

Most important early gate is M3 because it isolates coordinate truth before camera motion obscures the defect.

---

## 11. Mandatory read order for a fresh thread

A new thread/agent must read, in this order:

1. `AGENTS.md`
2. `docs/PROJECT_DIRECTION.md`
3. `docs/P4_1_STABLE_BASELINE.md`
4. `docs/P5_STABLE_BASELINE.md`
5. `docs/ARZOOM_MULTI_CANONICAL_IMPLEMENTATION.md`
6. **this file**
7. Issue #25
8. Draft PR #34

Do not start by reading/continuing PR #27 code as if it were current architecture.

---

## 12. Per-milestone completion record format

At the end of every implementation milestone, append/update this file with:

```text
Milestone completed:
Commit SHA:
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
Next ONE milestone:
```

If direct OBS evidence is pending, say `PENDING`; do not label milestone accepted.

---

## 13. Conversation-limit recovery procedure

If a thread is near context/token limit:

1. stop before starting a new milestone;
2. update this handoff file with exact current status, tests, blockers, and next one milestone;
3. update the Draft PR body if architecture/evidence changed materially;
4. commit/push the handoff update;
5. start a new thread and instruct it to read the mandatory read order above;
6. the new thread must verify PR #34 branch/head before editing.

Never rely on “the previous assistant remembers it.”

---

## 14. Things a future thread must not do

- do not merge/reopen PR #27 architecture;
- do not rename the old internal ID `arzoom_filter`;
- do not put Multi ownership logic back inside Single;
- do not implement M4 before M3 direct mapping proof;
- do not add per-source zoom;
- do not add a second planner/camera authority;
- do not add CPU readback/OCR/vision;
- do not add scene-item transform mutation as the camera mechanism;
- do not add per-frame scene enumeration/settings writes/log formatting;
- do not add an unbounded worker queue;
- do not weaken stable P0–P5 tests;
- do not touch Issue #26 while Issue #25 work is scoped here.

---

## 15. Current next action

Strategy reset is complete and PR #34 is the canonical Draft PR.

The next implementation action is exactly:

> **M0 — Dual-filter registration boundary**

Nothing beyond M0 should be bundled into that first runtime slice.
