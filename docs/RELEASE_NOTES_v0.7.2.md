# ArZoom for OBS v0.7.2

Native smart camera and presentation feedback for OBS Studio on Windows x64.

## New

- Premium single luminous click feedback with bounded same-target double-click
  retrigger. Mouse clicks **do not** trigger automatic zoom.
- Optional physical shortcut keycaps, with ordinary typing hidden.
- Compact native OBS settings: shorter labels, contextual tooltips, Spotlight
  disclosure, and advanced options.
- The accepted P4.1 camera and P5 cinematic Spotlight architecture remain in
  place, including Zoom +/- resize-only behavior.

## Reliability and privacy

- No new scene-camera authority, full-screen render pass, raw input
  registration, network service, key history or clipboard processing.
- Keyboard observation is opt-in and scoped to visibly used filters; it is
  unregistered on filter deactivation, disabling and module shutdown.
- OBS shader parameter fallback protects older effect ABI from disabling the
  core camera. A missing keycap atlas falls back safely.

## Installation

Close OBS. Install `ArZoom-OBS-Setup-windows-x64.exe` using the detected
standard OBS or custom/portable OBS root. Manual ZIP and SHA-256 hashes are
available in the release. Always back up a working installation before
upgrading.

## Compatibility and validation

Platform: Windows 10/11 x64; OBS Studio build baseline 31.1.1. The
maintainer-approved 1080p60 screen recording shows live Click and Keycap
presentation in OBS 33.0.1 (12.37 seconds, 1920×1080/60 FPS). Windows CI
completed 23/23 deterministic tests and six-case release guard checks,
and built the x64 DLL, ZIP and EXE installer.

This is a scoped field acceptance, **not** a measurement of plugin-only GPU
overhead, 4K latency, clean portable installation or long-duration stability.
These remain open for community verification. See
`docs/RELEASE_ACCEPTANCE_v0.7.2.md`; report regressions through GitHub
Issues. The source and issue tracker are available in this repository.

ArZoom is licensed under GPL-2.0-or-later.
