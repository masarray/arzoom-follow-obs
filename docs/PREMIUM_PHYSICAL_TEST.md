# ArZoom Premium — Windows OBS Physical Tester (v0.7.1 trial)

**Audience:** Windows OBS tester; this is an **Actions artifact**, not a public stable release.

## Source / scope

- Candidate PR: [Physical OBS trial PR](https://github.com/masarray/arzoom-follow-obs/pulls?q=is%3Apr+is%3Aopen+premium+physical).
- Includes the accepted v0.7.0 P4.1/P5 camera, Presenter, Spotlight and cursor behavior; P4.1 Single, not Multi.
- New: Arssyut-style single luminous ring and optional physical shortcut keycap overlay.
- **Explicitly excluded:** Arssyut click-triggered auto zoom. A mouse click must not activate Zoom.
- Default shortcut overlay: **OFF**; opt-in only. No keyboard history, clipboard or text decoding.

## Obtain and install

1. Open the candidate PR's latest **Build Windows** run in the Checks tab or the linked Actions run. Download artifact **`arzoom-windows-x64`**. GitHub may require sign-in.
2. Extract the downloaded Actions ZIP. Find **`ArZoom-OBS-Setup-v0.7.1-windows-x64.exe`**; a manual **`arzoom-obs-v0.7.1-windows-x64.zip`** is also packaged.
3. Close OBS completely. Back up existing ArZoom installer/ZIP if you need to roll back to v0.7.0. Run the setup and select the detected OBS installation or the actual OBS Portable root.
4. Restart OBS and confirm the log contains **`[ArZoom] BUILD v0.7.1-premium-physical-trial`**. If the log reports the previous build identity, another copy of the plugin is loaded.
5. In a test scene, attach the usual ArZoom filter to Display Capture, or use the established ArZoom Scene Camera flow. **Do not add ArZoom Multi to this acceptance test**. In filter properties, turn **Show shortcut keycaps (optional)** ON.

## Physical acceptance checks

| Test | Expected |
|---|---|
| Normal typing in a document, including Shift+letters and AltGr symbols | **No keycaps** |
| Ctrl+S, Ctrl+Shift+S, Win+E, F12 | Canonical compact physical keycaps appear bottom center |
| Rapid Ctrl+S then Ctrl+V | A new chord cleanly replaces prior; no stuck keycaps |
| Hold Ctrl+S, release, type regularly | No per-frame keycap re-raster; overlay disappears after ~1.49s |
| Normal click, rapid double click on one target | One expanding Sky/Rose/Amber luminous ring per target; double click recharges one pulse |
| Click with Zoom off | **Zoom remains off** |
| Zoom Follow with/without keycaps, plus existing Zoom +/- / Spotlight | Camera behavior unchanged; keycaps remain screen-space |
| 1080p60 & 4K if available, white document and dark desktop | Visible ring/keycaps, no black preview, no flicker, no clipped key labels |
| Turn keycaps OFF, switch scenes, close/restart OBS | Hook disabled when no subscribed filters; no stale overlay or crash |
| OBS portable/installed setups | Correct plugin build identity and data `arzoom.effect` loaded from same installation |

**Known OS limitation:** A low-level Windows observer cannot capture protected secure desktops or some system-reserved sequences. Shortcuts simulated by external tools may be intentionally ignored. Pressing a normal letter should never produce an overlay.

## Observation and rollback

Please preserve a **screen recording of the OBS Program output**, not merely the Properties sheet, and include OBS log lines around any shader/graphics errors. Compare identical camera/click traces to stable v0.7.0. If a first-frame black preview or app freeze occurs, turn off the keycap property, stop recording, restore the stable v0.7.0 installer, and attach the OBS log to the PR. Do not assume that Windows CTest success proves visual correctness.

## Pre-release engineering gates

- Windows CI C++ compile, full CTest, installer ZIP/EXE packaging and uploaded artifact.
- OBS runtime shader compilation, one first-frame pass, actual D3D11/OpenGL visual evidence, and measured GPU p50/p95 remain **direct hardware gates**.
- No auto-merge or public release until physical acceptance.
