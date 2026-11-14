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

Platform: Windows 10/11 x64, OBS Studio baseline 31.1.1. Direct Windows OBS
release acceptance is recorded in
`docs/RELEASE_ACCEPTANCE_v0.7.2.md` and must be approved before publishing.
The source and issue tracker are available in this repository.

ArZoom is licensed under GPL-2.0-or-later.
