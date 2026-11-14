# ArZoom v0.7.3 — Windows stable release acceptance

ReleaseGate: APPROVED

Approval date: 2026-11-15 (Asia/Jakarta)
Approval source: maintainer explicitly requested "oke bagus! release publik stable" after receiving the extended Spotlight / Presentation Cursor Windows tester build. This is a **scoped public-release decision**, not proof that a new direct-OBS session was recorded.

## What is approved

- Windows 10/11 x64 stable release **v0.7.3** from the reviewed candidate runtime commit `1f72321317138dc0a19cc71cd7209a9e5a3ff916` (PR #45), incorporating cursor work from PRs #41–#44.
- Thirteen cursor presets including the original, new **Premium Hand Chrome** default, 3D Hand Pointer, Cartoon Hand Pointer and Blue Sleeve Hand with complete cylindrical cuff.
- Spotlight Focus Area **50–400%**, retaining 170% default; Cursor Size **24–160 px**, retaining 52 px default; rendered cursor cap **512 px** with continuous hotspot positioning.
- Original camera/mapping ownership, click ring, opt-in shortcut keycaps, GPU atlas rendering and saved style selections are preserved.

## Verified automated and inherited evidence

- [x] [Windows candidate build #43645833252](https://github.com/masarray/arzoom-follow-obs/actions/runs/43645833252): deterministic motion validation, bundled cursor asset integrity checks, release-gate checks, OBS DLL build and Windows installer EXE/manual ZIP packaging — **SUCCESS**.
- [x] [Public v0.7.3 RC1 build #43644076602](https://github.com/masarray/arzoom-follow-obs/actions/runs/43644076602): package and public pre-release completed successfully, including the existing cursor preset stack.
- [x] Earlier v0.7.2 user-supplied **1920×1080, 60 FPS, OBS 33.0.1** video showed the retained click/keycap behavior in real OBS. This earlier video **does not validate** the new cursor styles or expanded Spotlight/size ranges.
- [x] Maintainer explicitly requested stable publication following these candidate builds, accepting the remaining disclosed scope limitations.

## Not directly verified for this exact release

- [ ] No new owner-supplied OBS recording or screenshot was provided demonstrating Blue Sleeve Hand hotspot/click feedback at 1×–4×.
- [ ] No physical OBS recording was provided demonstrating cinematic Spotlight 250–400% or enlarged 160 px cursor at 1×–4×.
- [ ] GPU p50/p95 / 4K load, multi-hour soak, multiple filter instances, and isolated plugin CPU/GPU overhead remain unmeasured.
- [ ] Clean install/upgrade/rollback matrix across OBS 31.x–33.x, portable installations, OpenGL/D3D11, multiple GPUs and bilingual HiDPI remains incomplete.
- [ ] Independent verification of source-image redistribution rights has not been supplied. New hand illustrations were based on maintainer-provided image references and should be reviewed in case of third-party rights claims.

The maintainer authorizes this **bounded stable release with the above disclosed unknowns**. This is not a claim of universal compatibility or unperformed direct OBS tests; community feedback and rollback to v0.7.2 remain supported.

## Release safeguards

Publish only after version-matched stable README, landing page, setup guide, release notes, buildspec and installer checks. GitHub release must be triggered from current `main` using version-locked `release/v0.7.3` or matching `v0.7.3` tag, not an arbitrary feature push. The Windows workflow must build and upload `ArZoom-OBS-Setup-windows-x64.exe`, `ArZoom-OBS-Manual-v0.7.3-windows-x64.zip` and `SHA256SUMS.txt` before claiming public completion.

The single approval marker above is the maintainer's explicit release instruction. Approval does not convert unchecked validations into passing evidence.
