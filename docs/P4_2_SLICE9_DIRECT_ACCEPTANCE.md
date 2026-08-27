# ArZoom P4.2 Slice 9 — Direct OBS Dual-Screen Acceptance

**Issue:** #25 only  
**Build under test:** `dc8c4e662d177d3e82042948da4df70c072583ad`  
**CI:** Build Windows #276 — PASS  
**Expected artifact SHA-256:** `8c7ad657dae6e443f33c9e8de9272c3bece77b00d97c599275a7268d5a9af9e1`

## Test scene

Use two physical monitors.

- Monitor A: Coding / IDE
- Monitor B: Running app / browser
- OBS scene: two top-level Display Captures, side-by-side or otherwise independently mapped
- Optional third Display Capture: OBS / utility; leave it unchecked in Presentation Screens

Attach the managed scene-level **ArZoom Camera** as in the existing P4.1/P5 workflow.

## Pre-test

1. Close OBS.
2. Install `ArZoom-OBS-Setup-v0.7.0-windows-x64.exe` from the CI artifact.
3. Start OBS.
4. Open the test scene and ArZoom Properties.
5. In **Presentation Screens**, select Monitor A and Monitor B captures.
6. Leave utility/OBS capture unchecked.
7. Press **Refresh status**.
8. Confirm there are no duplicate native captured cursors if using Presentation Cursor.

## Required acceptance matrix

Mark each row PASS or FAIL.

| ID | Test | Expected |
|---|---|---|
| A1 | Properties opens | No flicker, no crash; Presentation Screens group visible |
| A2 | Select A+B | Both rows show selected; utility stays unselected |
| A3 | Close/reopen Properties | Selection remains |
| A4 | Restart OBS | A+B selection persists |
| A5 | Rename selected source | Same UUID remains selected |
| A6 | Delete/recreate same-name source | New UUID does **not** silently inherit selection |
| B1 | Zoom ON, cursor on A | Coding region becomes readable |
| B2 | Move cursor A → B | Same scene-level camera travels smoothly to B |
| B3 | Move B → A | Camera returns smoothly; no reset/snap |
| B4 | Repeat A↔B 10 times | No border chatter, oscillation, or dead zone |
| B5 | Cursor on unchecked utility monitor | No nearest-screen guess; camera state remains safe/stable |
| C1 | Left-click ring on A | Lands on pointer content |
| C2 | Left-click ring on B | Lands on pointer content |
| C3 | Classic Hand on A/B | Lands on pointer content |
| C4 | Presentation Cursor on A/B | Lands on pointer content |
| C5 | Spotlight Follow Cursor A/B | Lands on same active mapping |
| C6 | Spotlight Click A/B | Lands on click anchor |
| C7 | Spotlight Smart Focus A/B | Uses same active mapping |
| D1 | Toggle Zoom ON/OFF while crossing | Cinematic close/open remains correct |
| D2 | Zoom +/- while active | Resize-only; no cinematic replay |
| D3 | Hide selected capture | Fail-safe; no guessed replacement |
| D4 | Restore same selected source | Recovers when same UUID is available |
| D5 | Scale/inset one capture | Mapping remains spatially correct |
| D6 | Supported crop on one capture | Mapping remains spatially correct |
| E1 | Video output | No black frame |
| E2 | OBS Properties | No runtime flicker during cursor crossing |
| E3 | OBS log | No D3D11 unset-shader / ArZoom render errors |
| E4 | Performance | No obvious new lag/stutter during repeated crossing |

## Boundary decision

- If **B4 PASS**, **Slice 7 stays skipped**.
- If **B4 FAIL only because of real boundary chatter**, stop acceptance and implement the smallest possible Slice 7 hysteresis, then repeat Slice 9.
- Do not add hysteresis for unrelated failures.

## Evidence required before acceptance

Return:

1. this matrix with PASS/FAIL marks;
2. one screenshot of Presentation Screens Properties;
3. OBS log from the trial;
4. if any row fails, a short screen recording or exact reproduction steps.

A local helper script in the Slice 9 acceptance pack collects monitor bounds, installed ArZoom DLL hash, latest OBS log, and a targeted ArZoom/D3D11/shader/error scan.

Do not mark Issue #25 stable-candidate until every required row passes.
