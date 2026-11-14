# ArZoom v0.7.2 — owner-approved Windows stable release

ReleaseGate: APPROVED

Approval date: 2026-11-14 (Asia/Jakarta)
Approval source: maintainer explicitly accepted the tested plugin and requested
stable public release after reviewing the recording and screenshot.
This is a **scoped release decision with disclosed limits**, not a claim that
all laboratory performance gates were measured.

## Confirmed evidence

- [x] Premium click and physical shortcut keycaps previously accepted by
  maintainer as functioning well; compact native GUI accepted.
- [x] Maintainer supplied a 12.3667-second MP4 recorded at **1920x1080, 60 FPS**,
  showing OBS presentation camera framing, one pink luminous mouse click ring,
  and bottom-centered physical-style shortcut keycaps.
- [x] Supplied Task Manager screenshot identifies **OBS Studio 33.0.1** on
  Windows, approximately **105.4 MB** process memory and **0.6% CPU** at the
  particular snapshot. These figures are **not plugin-only** performance.
- [x] Candidate runtime source commit
  `041d8e5082d5c47d3e8a2f051ac6f263e859f7d9`
  passed Windows Actions [run #43596962392] with **23/23 CTest PASS**,
  six fail-closed release-approval cases, Windows x64 DLL, ZIP and EXE installer
  packaging PASS.
- [x] The stable release code retains the same premium Click, Keycaps,
  Smart Follow and Spotlight runtime code as that candidate; subsequent
  updates before merge are documentation, website and release approval only.
- [x] User expressly approved proceeding with the public stable release.

## Not independently verified — disclosed release limitations

- [ ] Isolated GPU frame time p50/p95 or 4K GPU performance relative to v0.7.0.
- [ ] Multi-hour OBS soak test, stress with many concurrent ArZoom instances.
- [ ] Fresh-install/rollback evidence across both standard and portable OBS.
- [ ] Full matrix for OBS 31.x, 32.x, OpenGL, bilingual HiDPI and legacy shader
  mismatch, including independently captured OBS error logs.
- [ ] Ordinary typing/AltGr/Shift-only privacy matrix on multiple keyboard
  layouts in a physical Windows session; deterministic policy tests have
  passed, but this is not the same as full hardware coverage.
- [ ] Specific source-only CPU/GPU/memory baseline on the Task Manager screenshot.

The maintainer accepts these remaining compatibility/performance risks for
a Windows x64 **v0.7.2 stable** release and invites user reports. Do not
retroactively mark the untested items checked. Do not advertise verified 4K
performance, a zero-overhead claim, or universal support.

## Release publication safeguards

Public publishing is an **explicit release action**, never an ordinary
`main` push. It may be launched by manual workflow dispatch on `main`
(`approve_stable_release=true` and matching `tested_commit_sha`), a
version-matched `v0.7.2` tag, or the one-off, version-locked release branch
`release/v0.7.2`. The tag/branch must point to current `main` HEAD.
All paths validate this one owner-approved release gate, reviewed release
notes, matching ZIP/EXE installer and public website/README version. The
release branch is intentionally version-locked so ordinary pushes or other
branches cannot publish. The runtime code has not changed since the
field-accepted build.

The single `ReleaseGate: APPROVED` line at the top of this file is the
maintainer's explicit release decision. The CI-tested release-policy guard
rejects duplicate or conflicting gate lines. README, landing page, guide
and release notes must advertise the exact final version and limitations.

- Pre-approval candidate runtime commit:
  `041d8e5082d5c47d3e8a2f051ac6f263e859f7d9`
- Evidence reviewed: user-supplied `2026-11-14 20-04-38.mp4` and OBS Task
  Manager screenshot (retained in the conversation, **not published publicly**)
- OBS version visible: **33.0.1**; video: 1920×1080 / 60 FPS
- Public release version: **v0.7.2**, Windows x64
- Final release merge SHA: filled by GitHub when the merge is completed
- GPU p50/p95: not provided; outstanding for follow-up validation
