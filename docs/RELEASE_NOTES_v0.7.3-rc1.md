# ArZoom v0.7.3 RC1 — Public Cursor Preview

**Public pre-release for Windows 10/11 x64, not the current stable release.**
Stable ArZoom v0.7.2 remains the recommended production version.

## What is new

- New **Blue Sleeve Hand** — glossy pointing hand with a **complete blue cylindrical sleeve**, including the shaded interior. Transparent, lightweight 160×160 PNG; calibrated fingertip hotspot.
- Optional **Cartoon Hand Pointer**, **3D Hand Pointer**, **Premium Chrome Arrow**, **Premium Hand Chrome** and original **Premium Hand** alongside all legacy procedural choices, for a total of 13 presets.
- On new filters, **Premium Hand Chrome remains the default**; previously saved cursor selections and Off are not changed.
- Retained original 28-frame, 260 ms tactile click animation and single shared GPU atlas path. PNG decoded only on preset selection, not on video frames.

## Installation and use

1. Close OBS Studio. Back up any working configuration before changing the installed plugin.
2. Download and run `ArZoom-OBS-Setup-v0.7.3-windows-x64.exe`, or use the manual ZIP if you manage an OBS portable install.
3. In ArZoom filter Properties choose **Pointer → Style → Blue Sleeve Hand**. Recommended size: **80–96 px**.
4. For a single cursor, disable the built-in **Display Capture → Capture Cursor** option when ArZoom Presentation Cursor is enabled.

Check the accompanying SHA-256 checksum file to verify the downloaded package.

## Verified for this preview

- Windows x64 Actions builds the OBS plugin DLL and EXE/ZIP installer.
- Deterministic cursor preset/hotspot tests, image hash/dimensions/alpha and 32/48/64-pixel silhouette gates, and stable-release policy regression tests run in CI.
- Build uses the same camera/planner authority and does not introduce another render pass, CPU frame readback or runtime PNG decoding.

## Not yet verified

- **No owner-supplied recording or screenshot proving the new cursor in a physical OBS session is attached to this preview.**
- Test fingertip alignment with the click ring at 1×, 2× and 4× zoom, across light/dark captures and after OBS restart.
- Clean installations, 4K/GPU overhead, multi-hour soak and full hardware/OBS version compatibility remain unproven.
- Artwork was recreated from images supplied during development; confirm the rights to redistribute any third-party reference-derived elements before promotion to a stable/publicly recommended release.

This pre-release is intended for user feedback, not an assertion of stable field acceptance. Report issues at https://github.com/masarray/arzoom-follow-obs/issues .

## Source and dependencies

Source revision is pinned by tag `preview-v0.7.3-rc1`. The cursor development stack is PRs #41–#44; stable v0.7.2 remains unchanged. Licensed project source: GPL-2.0-or-later.
