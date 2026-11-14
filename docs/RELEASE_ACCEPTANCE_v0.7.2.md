# ArZoom v0.7.2 public release acceptance

ReleaseGate: BLOCKED

This gate deliberately remains BLOCKED until a maintainer has completed
**direct OBS Windows acceptance** of the *exact* release commit and checked
all items below. CI passing is necessary, but not sufficient.

## Required evidence

- [ ] Windows OBS 31.x and current supported OBS version both load the same
  plugin DLL and matched data/effects/arzoom.effect without parameter warnings.
- [ ] 1080p60 direct OBS Program recording, white/dark backgrounds, confirms
  the accepted single luminous ring, double-click recharge and camera isolation.
- [ ] Shortcut keycaps: default OFF; opt-in ON, Ctrl+S, Ctrl+Shift+S, Win+E,
  F12; ordinary typing, Shift+letters and AltGr text remain hidden.
- [ ] Toggle filter/scene enable, hide/show scene, remove filter, disable feature,
  and OBS shutdown/restart: no stale shortcut replay, unexpected keyboard
  observer, crash or black output.
- [ ] Full-screen and scene-wide Smart Follow, Zoom +/- and Spotlight with
  click/keycap both enabled and disabled: no changed P4.1/P5 behavior.
- [ ] Native OBS v0.7.2 compact UX verified at normal/HiDPI sizes:
  Spotlight OFF/ON, Advanced OFF/ON, informative warnings, retained settings,
  EN/ID locale; no unexpected hidden actions or cursor double-overlay.
- [ ] D3D11 direct first-frame render (and OpenGL if claimed supported):
  zero shader-ABI errors, unbound sampler errors, flicker or black preview.
- [ ] GPU p50/p95 comparison with v0.7.0 at 1080p60 and 4K where available,
  with clear test scene/settings, demonstrates no significant regression.
- [ ] v0.7.2 installer and manual ZIP tested on clean standard OBS and portable
  OBS; uninstall and rollback to stable v0.7.0 tested.
- [ ] README, website, CHANGELOG, release notes and published installer naming
  verified against actual final feature set and version.
- [ ] Licenses/attributions, dependencies and source availability checked.

## Approval procedure

Once physical evidence and the final public documentation are attached to
the release review, the owner can change the exact line to:

Set the single status line at the top of this file to `ReleaseGate: APPROVED`.

That is a **manual decision** and must not be automatically changed by CI or
an assistant. Public publishing additionally requires an explicit GitHub
Actions workflow_dispatch with `approve_stable_release=true` on `main`,
and the `tested_commit_sha` input must match `main` at dispatch. This
avoids an impossible self-referential SHA recorded in this document.
Do not copy forward any old OBS direct-test claims to v0.7.2.

- Tested pre-approval artifact commit: PENDING
- OBS version / Windows GPU(s): PENDING
- Recording / OBS log reference: PENDING
- GPU p50/p95 evidence: PENDING
- Maintainer / date: PENDING
