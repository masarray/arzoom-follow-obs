# ArZoom Premium — code audit and public release gate (2026-11-14)

**Release audit conclusion: owner-approved scoped stable v0.7.2 on 2026-11-14 after the uploaded OBS 33.0.1 1080p60 functional clip, with explicitly documented unresolved 4K/GPU p50–p95 and clean-install test limits.**
Audited current development head on PR #39, stacked on user-accepted functional
checkpoint PR #38. `main` and public v0.7.0 remain unchanged. PR #34 ArZoom Multi
stays independent and is explicitly not part of the premium Single release.

## Findings and remediation

| Severity | Root observation in source | Correction on PR #39 |
|---|---|---|
| P0 | `phase24_create` subscribed to WH_KEYBOARD_LL immediately when the saved keycap checkbox was On, even if its filter was loaded but not showing | Register at live showing `video_tick` only, unsubscribe on deactivation, disabled setting and destruction. One module-level hook remains shared across instances |
| P0 | The v24 renderer skipped the **entire** source on missing optional keycap shader uniforms, rather than preserving the established camera/click pipeline | With absent optional keycap ABI, use inherited v23 presentation path and disable key observation. With a shader that actually declares the sampler but no safe fallback, retain pass-through safety |
| P1 | Plain `Timeline` frame/opacity was written during tick and read during render without an explicit phase synchronization contract | A short per-filter mutex protects fixed-capacity semantic snapshot only; rasterization, texture update and GPU draw stay outside the lock |
| P1 | Deactivate/reset cleared `last_generation` to zero, so reactivation could replay an already captured shortcut if another filter kept the shared observer live | Reset consumes the currently published generation; tests prove stale event suppression and fresh event acceptance |
| P1 performance | Starting the optional Win32 hook worker from `video_tick` synchronously waited for the input thread to create its message queue, potentially stalling an OBS frame on first opt-in | Observer startup is now nonblocking on the video tick. Only module shutdown waits for worker readiness and joins the thread; spawn failures fail closed and can retry on later ticks |
| P0 release | A push to `main` could automatically publish a stable release and release notes said `19/19` tests/direct OBS acceptance without evidence for the candidate | Manual-only `workflow_dispatch`, explicit `approve_stable_release` and `tested_commit_sha`, per-version approval doc with `ReleaseGate: APPROVED`, exact installer/ZIP files, reviewed notes and required asset presence |
| P2 | The CMake standalone version stayed at 0.7.0 when the Windows buildspec was 0.7.2 | Synchronize CMake project version and keep a v0.7.0 public-stable badge until a real public release |

## Architecture now protected

- Scene-level P4.1 `SceneViewportPlanner` (WHERE) and `SceneKinematicMotion` (HOW) remain untouched. No competing zoom/camera authority and **no click-triggered auto zoom**.
- Click V2 stays one analytic ring, four fixed events, bounded coalescing, and one GPU presentation pass.
- Keycap renderer keeps a fixed 960x144 retained atlas, updates only when the chord generation changes, and does not collect text/clipboard data.
- Optional Windows hook is default Off; only one per OBS plugin process. Full hook-chain forwarding is unchanged.
- Native OBS Properties remain compact, with existing settings keys and runtime presenter handlers unmodified.
- The release workflow now explicitly blocks before public publication without human-reported, commit-matched OBS field evidence. No automatic release on a merge.

## Release-blocking physical validation still needed

1. Fresh Windows x64 installer/ZIP on standard OBS and on portable OBS;
   verify exact v0.7.2 DLL/effect match in log and rollback.
2. Actual OBS Program capture at 1080p60 with Click V2 and Keycaps,
   including multi-instance opt-in, scene switch, filter hide/show,
   OBS restart and toolbar toggle.
3. Verify `obs_source_showing(filter)` is asserted for the intended normal
   preview/program routes so the opt-in keyboard overlay still works. A green
   native CTest cannot prove this.
4. 1080p and 4K GPU p50/p95 compared to the accepted baseline (same scene,
   GPU, driver, effects) and no shader errors/black output.
5. Spotlight OFF/ON, Cinematic Zoom +/- and keycap-only Draw paths.
6. Compact native GUI in EN/ID, Advanced OFF/ON, retained old settings,
   warning visibility, HiDPI, and no callback flicker.
7. Matched source release docs, website, changelog, hashes and complete
   physical-test evidence recorded in `RELEASE_ACCEPTANCE_v0.7.2.md`.

**CI** is necessary but cannot replace these field gates. The last previously
verified candidate passed 22/22 CTest and built Windows DLL/ZIP/EXE;
additional release-hardening regressions were added and require their own
fresh CI verdict before any promotion.

## Maintainability judgement

The inherited v2–v24 include-wrapper chain has significant historical
complexity. Broadly flattening or rewriting this chain just before a public
release would threaten accepted P4.1/P5 behavior; this audit deliberately made
small cohesive changes at v24 ownership and in isolated policy/tests instead.
Refactoring the historical wrappers is a separate evidence-driven task.

## Release procedure

1. Finish direct field tests and enter real recordings/logs/benchmarks.
2. Finalize README/website version badge and `docs/RELEASE_NOTES_v0.7.2.md`.
3. On `main`, commit `ReleaseGate: APPROVED` only after review.
4. Run the manual stable workflow on `main` with checkbox approval and
   `tested_commit_sha` equal to the exact GitHub SHA of the selected revision.
5. Verify release installer, ZIP, SHA256 and public landing-page link.
