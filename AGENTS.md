# AGENTS.md — ArZoom OBS Production Engineering Contract

This file is the operating contract for AI/code agents modifying ArZoom. ArZoom is a native OBS presentation-camera/effect plugin; render correctness, OBS stability, deterministic camera semantics, GPU-path safety, bounded work, and regression protection are product requirements.

## 1. Prime directive

Do not start with a disposable or intentionally naive implementation when the production architecture is already knowable.

Prefer the smallest coherent production-quality change. Preserve accepted behavior unless the task explicitly changes the contract.

Priority order:
1. OBS/render correctness and no black-frame/crash regression;
2. stable camera/mapping/presentation semantics;
3. bounded realtime/render cost;
4. regression compatibility;
5. maintainability;
6. implementation convenience.

## 2. Mandatory workflow

For non-trivial work:

RECONNAISSANCE -> REPRODUCE/BASELINE -> ROOT CAUSE -> INVARIANTS -> ARCHITECTURE IMPACT -> IMPLEMENT -> REGRESSION TEST -> FAILURE TEST -> PERFORMANCE CHECK -> WINDOWS BUILD -> DIRECT OBS VALIDATION

Before editing, read the current project direction and accepted stable-baseline documents referenced by README. Identify the owning subsystem and all downstream consumers before changing shared camera, mapping, render, hotkey, shader, cursor, or Spotlight behavior.

If three consecutive patches in the same subsystem are still treating symptoms, STOP. A fourth patch requires a fresh root-cause/state-ownership/architecture audit.

Do not revive superseded architectures merely because they look easier locally.

## 3. Architecture invariants

Unless an explicit approved task changes them, preserve:
- no persistent scene-item transform mutation for scene-wide camera behavior;
- no duplicate scene render graph;
- no CPU frame readback in the presentation hot path;
- one semantic camera/planner authority;
- one coherent Display Capture -> scene mapping authority;
- bounded O(1) presentation state;
- OBS pass-through when presentation effects are inactive;
- Spotlight/cursor/click effects remain presentation layers, not competing camera authorities.

Do not create parallel state models for zoom, focus, cursor, Spotlight, mapping, or hotkey intent.

## 4. Exception-free render/hot paths

Expected or recoverable failures must not use exceptions as normal control flow in OBS render callbacks, per-frame camera math, cursor/Spotlight animation, shader-parameter preparation, or other high-frequency paths.

Prefer compact explicit status/result contracts appropriate to C++ (`enum`/status structs, `std::optional` when failure detail is unnecessary, or one project result type where useful).

No exception may unwind through an OBS video/render callback or timing-sensitive presentation callback.

OS, filesystem, image decoder, Qt/OBS/helper-library, installer, or configuration exceptions may occur at non-realtime boundaries. Catch them at the nearest meaningful boundary and convert them into controlled application state or structured diagnostics.

Do not globally disable C++ exceptions merely to satisfy this rule. Use `noexcept` only when the complete reachable path genuinely honors it.

## 5. Bounded asynchronous diagnostics

Hot paths must not format long strings, write files, perform network telemetry, serialize JSON, or synchronously log repeated failures.

Use compact diagnostic events/counters containing stable error code + tiny numeric/context fields. Delivery must be bounded and non-blocking for render producers.

Repeated failures must be aggregated, deduplicated, or rate-limited. Queue saturation must have an explicit drop/coalesce policy. Diagnostics are observational: a slow/broken diagnostic consumer must never stall rendering, camera motion, hotkeys, or OBS.

Human-readable formatting and persistence belong outside the render path.

## 6. GPU/shader safety

Every shader parameter used by a shared draw path must receive deterministic valid values before draw. Never rely on stale GPU state or a parameter being irrelevant in one branch when the same shader is shared elsewhere.

Treat black preview/output as a P0 regression.

Avoid per-frame shader compilation, texture creation/destruction, image decoding, GPU readback, or filesystem access.

Prepare/rebuild expensive cursor atlases, textures, geometry, or configuration state only when inputs actually change, then publish coherent prepared state to the render path.

Failure to prepare a candidate resource must retain the previous valid resource or safe pass-through behavior.

## 7. Camera/mapping state

Mapping, camera intent, animation state, and presentation state must have explicit ownership.

Ambiguous Display Capture ownership/geometry must fail safe rather than guess. Negative desktop coordinates, scaling, crop, inset/fullscreen placement, and zoom transforms must preserve one mathematically coherent mapping chain.

Do not fix mapping defects with arbitrary offsets specific to one monitor screenshot.

Hotkeys and click/cursor presentation effects may request state transitions, but they must not mutate competing hidden camera truth.

## 8. Timing and animation

Presentation animation must be time-based, bounded, reversible where the accepted contract requires it, and independent of accidental frame-rate assumptions.

Do not solve races with arbitrary `Sleep`/delay calls on render/UI paths.

Per-frame work must remain bounded and should not grow with session duration. Histories/queues must never grow without an explicit cap.

## 9. Threading and lifecycle

OBS callbacks, hotkeys, settings/property UI, platform hooks, texture/resource ownership, and background helpers must have explicit lifetime owners.

Shutdown/reconfigure must prevent callbacks into destroyed state and release hooks/resources deterministically.

Do not detach threads to avoid lifecycle work. Do not hold blocking locks across OBS render callbacks.

Cross-thread state consumed by rendering should use bounded snapshots/atomics/double-buffered prepared state as appropriate rather than mutable shared graphs guarded by long locks.

## 10. Result-oriented boundaries

Expected states such as unsupported capture geometry, missing optional asset, invalid settings, unavailable hook, failed candidate texture, or unresolvable mapping should return an explicit status and deterministic fallback.

Use one coherent error taxonomy per related subsystem. Do not create a unique Result class for every function.

Configuration/state promotion is transactional:
VALIDATE -> BUILD/PREPARE CANDIDATE -> VERIFY -> COMMIT -> otherwise RETAIN LAST-KNOWN-GOOD.

## 11. Performance discipline

Do not claim an optimization without evidence. Measure relevant before/after behavior where a hot path changes, including as applicable:
- render/frame CPU time;
- GPU work/pass count;
- allocation rate;
- texture/resource churn;
- callback frequency;
- memory growth during long sessions;
- hotkey-to-visible-response latency.

Prefer bounded math and retained/prepared resources over speculative worker threads or caches.

A visualizer/effect enhancement is not successful if it materially harms OBS responsiveness or rendering stability.

## 12. UI/property behavior

OBS property UI must remain configuration/presentation only; it must not become a second runtime engine.

Avoid refresh loops or property rebuilds for state that can update without rebuilding the full properties surface. UI status must reflect authoritative runtime state, not merely the last requested toggle.

Do not add cosmetic animation that increases render/UI load without product value.

## 13. Regression protection

Every bug fix should add/update a deterministic regression test where technically practical. Test the exact failure mode, not just a nearby happy path.

Before changing shared mapping/camera/shader behavior, run the existing phase validation because later presentation layers depend on earlier stable baselines.

Preserve direct-OBS acceptance requirements for changes that automated tests cannot prove, especially D3D11 rendering, cursor hotspot alignment, camera motion, black-frame safety, and installer behavior.

## 14. Change discipline

Do not:
- mix unrelated refactors into a focused fix;
- create a second camera/mapping/render authority;
- rewrite accepted stable subsystems because a local patch is hard to understand;
- add per-frame logging/allocation/file access;
- add dependencies without evaluating binary size, licensing, runtime cost, and OBS compatibility;
- weaken deterministic gates merely to make a new feature pass.

## 15. Definition of done

A task is not complete because CMake compiles.

Validate as applicable:
DETERMINISTIC PHASE TESTS
+ REGRESSION TEST
+ NEGATIVE/FAILURE TEST
+ WINDOWS C++/SHADER BUILD
+ PACKAGE/INSTALLER CONTRACT
+ PERFORMANCE/RESOURCE CHECK
+ DIRECT OBS VALIDATION

Never claim a check was run when it was not.

## 16. Completion report

Report:
- changed;
- demonstrated root cause;
- architecture/state-owner decision;
- invariants preserved;
- Result/failure contract affected;
- regression protection;
- measured performance impact where relevant;
- exact validation executed;
- remaining genuine limitations.

## Final rule

Think like the engineer responsible for keeping OBS stable during a live presentation, not like a prototype author trying to make one screenshot pass. Preserve one camera authority, keep render work bounded, fail safe when mapping is ambiguous, and require evidence before calling a change complete.
