# ArZoom for OBS v0.7.3 — Windows Stable

Native OBS Studio presentation camera and on-screen cursor, click, shortcut and Spotlight feedback for Windows 10/11 x64.

## New in v0.7.3

- **Spotlight Focus Area: 50–400%**, expanded from 200%, with 5% steps. The default remains 170%. At the largest sizes the illuminated area may fill most of the canvas.
- **Presentation Cursor size: 24–160 px**, expanded from 96 px, following camera Zoom with a maximum of 512 rendered output pixels. At extreme magnification 160×160 PNG sprites may appear softer.
- **13 cursor presets** including Blue Sleeve Hand (complete shiny blue cylindrical cuff), Cartoon Hand Pointer, 3D Hand Pointer, Premium Chrome Arrow, Premium Hand, Premium Hand Chrome and existing procedural choices.
- **Premium Hand Chrome** remains the default for new filters; existing selected styles and Off remain unchanged.
- 28-frame / 260 ms animated click response reuses the existing shared GPU atlas without per-frame asset decoding or an added fullscreen render pass.

## Windows download and installation

1. Close OBS Studio. Back up your working installation if upgrading.
2. Run `ArZoom-OBS-Setup-windows-x64.exe`. Manual installs can use `ArZoom-OBS-Manual-v0.7.3-windows-x64.zip` for a standard or portable OBS plugin directory.
3. In ArZoom filter Properties, choose **Pointer → Style → Blue Sleeve Hand** (try 80–96 px, expandable to 160 px).
4. For Spotlight, change **Focus Area** from 50% to 400%; try 250–300% first. The default remains 170%.
5. Disable the native **Display Capture → Capture Cursor** setting when using ArZoom's replacement pointer to avoid duplicate cursors.
6. Compare downloads to the accompanying `SHA256SUMS.txt` file.

## Validation and known limits

The candidate passed [Windows build #43645833252](https://github.com/masarray/arzoom-follow-obs/actions/runs/43645833252), including deterministic tests, PNG integrity and safety checks, GPU plugin DLL build, installer EXE and ZIP packaging. The [v0.7.3 public preview build](https://github.com/masarray/arzoom-follow-obs/actions/runs/43644076602) also passed. Existing click/keycap functionality has prior direct OBS evidence from v0.7.2 (OBS 33.0.1/1080p60).

**No new direct-OBS recording was supplied for the new 400% Spotlight / 160 px pointer or 3D/Cartoon/Blue Sleeve cursor variants.** Isolated GPU frame-time numbers, 4K stress, multi-hour soak, extensive clean-install matrices and independently verified image-reference redistribution rights also remain outstanding. The maintainer explicitly approved a Windows stable release with these disclosed limitations; see [acceptance record](https://github.com/masarray/arzoom-follow-obs/blob/main/docs/RELEASE_ACCEPTANCE_v0.7.3.md).

Please report any regression at https://github.com/masarray/arzoom-follow-obs/issues. [Stable v0.7.2](https://github.com/masarray/arzoom-follow-obs/releases/tag/v0.7.2) remains downloadable for rollback. ArZoom source code is GPL-2.0-or-later.
