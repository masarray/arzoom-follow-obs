# ArZoom Premium — Native OBS Properties UX

**Status:** physical UX trial, based on the user-accepted click/keycap v0.7.1 checkpoint (Draft PR #38). Scoped to OBS Properties and localization. No click auto-zoom.

## Why this exists

The OBS Filters dialog was overwhelmed by long embedded explanations, duplicate Hotkey entry points, an alarming yellow status for optional bindings, technical copy, and Spotlight settings showing even when Spotlight master was Off. On a ~860px-wide screenshot, users needed to scroll through several paragraphs before they could see shortcut keycaps.

Research grounding: Nielsen Norman Group *Progressive Disclosure*, Microsoft's *User Interface Text* guidelines, and the official OBS Properties API (native groups, visibility and modified callbacks).

## UX contract

**Default user view — zero tutorial paragraphs:**

- Enable smart camera; Zoom; Follow; Motion; Follow range; Preview and Reset.
- **One** Keyboard shortcuts… entry point.
- Click feedback — existing enable control, no technical GPU explanation.
- Pointer — Style and Size; Custom import appears when Custom is selected.
- Spotlight — only the Enable checkbox until enabled.
- Shortcut keys — one opt-in checkbox.
- Show advanced options — OFF by default.

**When Spotlight enabled:** reveal Animate with zoom, Focus follows, Focus area, Background dim, Toggle spotlight and Preview spotlight. Animation speed, Shape and Edge softness are shown **only when Advanced options is enabled**. These are still the original runtime settings and OBS keys.

**When Advanced enabled:** expose Display, Focus anchor X/Y, Reset when hidden, Save shortcuts; and advanced Spotlight controls. Hiding them never resets values. The advanced flag is UI-only per-source OBS data, default Off.

**Contextual help:** brief native long-description tooltips on Follow, Follow range, Click feedback, Pointer, Spotlight, Shortcut keys, Advanced and Hotkeys. Never print technical explanations as read-only panels.

**Required alerts remain visible:** shader/effect errors; custom pointer asset loading failure; warning if the underlying Display Capture still captures its own pointer.

## Engineering invariants

1. Accepted v0.7.1 premium single-ring click GPU effect, double-click coalescing, 21 existing test gates, keycap hook, observer, privacy logic and retained rasterizer remain untouched.
2. P4.1 camera WHERE/HOW, P5 cinematic Zoom +/- and all presenter hotkeys untouched. **No Arssyut click auto-zoom**.
3. `phase24_properties` is the only UX adapter entry point; inherited phase constructors retain all property owners, callbacks and settings IDs. No new v25 wrapper.
4. UI visibility callbacks may return `true` to reflow on editing Spotlight or Advanced, per OBS API. Existing runtime Toggle/Peek buttons continue to return `false`, avoiding refresh/flicker.
5. Both en-US and id-ID use the same keys and concise copy. No external Qt webview or custom widget.
6. The old long read-only text properties remain in the inherited tree but are hidden at the final adapter boundary, so no phase engine code is rewritten and important errors can still appear.

## Physical acceptance checklist

- OBS 31.x Windows x64 (same environment as v0.7.1 checkpoint); test **English and Indonesian** if installed.
- With defaults, verify 1 screen can be scanned without reading a paragraph. Click and keycaps still function exactly as on accepted v0.7.1.
- Set Spotlight On: the focus controls appear; turn Off: they disappear again. Saved Shape/Softness/Speed values must be retained.
- Toggle Advanced On/Off while Zoom is active: no camera reposition, no cinematic iris restart, no black preview and no interrupted keycap.
- Spotlight Toggle/Preview buttons: remain responsive with no OBS Properties rebuild/flicker.
- Select Custom pointer: custom asset controls remain accessible even with Advanced Off.
- Force a missing cursor asset and duplicate native Display Capture cursor: error/warning text stays visible.
- Check 860px and 1280px wide Properties dialogs, high DPI where possible, and no truncated settings or inaccessible options.
- Record screen and OBS log. Automated CTest/Windows installer only prove build and unit behavior, **not native OBS visual QA**.

## Download

Once this branch's Build Windows Action succeeds, download the `arzoom-windows-x64` artifact and install `ArZoom-OBS-Setup-v0.7.2-windows-x64.exe` with OBS closed. Reopen and confirm `[ArZoom] BUILD v0.7.2-premium-compact-ux-trial` in the OBS log. Keep the accepted v0.7.1 installer as rollback.

