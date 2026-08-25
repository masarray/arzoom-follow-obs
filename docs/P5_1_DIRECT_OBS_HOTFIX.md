# P5.1 — Direct OBS Spotlight Hotfix Contract

**Status:** implementation candidate on `feature/p5-spotlight`; must remain Draft until a second direct-OBS trial passes.

**Trigger:** first direct OBS trial of the P5 Spotlight build on 2026-08-26.

This document records the observed failures and the accepted correction strategy. It is intentionally narrower than `P5_SMART_FOCUS_SPOTLIGHT_UX.md` and takes precedence for P5.1 lifecycle/render-safety behavior until the main P5 document is reconciled.

## 1. Direct-OBS findings

The first Spotlight trial proved the analytic GPU mask visually, but exposed five product/runtime defects:

1. clicking the captured screen could briefly produce a blank/black Display Capture frame, reopening the first-click regression previously fixed in Phase 4;
2. enabling Spotlight in properties immediately put the mask on-air even when the presenter had not requested Spotlight;
3. the primary tutorial shape was missing a true Circle option;
4. the GUI did not expose one obvious continuous control to enlarge/shrink the focus area;
5. immediately after filter startup, switching Smart Focus / Cursor / Click / Smart could retain stale focus state until Preview Spotlight was toggled.

These are release blockers. P5 must not leave Draft while any of them remain reproducible.

## 2. Correct activation semantics

`spotlight_enabled` is a **feature availability/master configuration**, not presenter on-air intent.

The mask is visible only when all required runtime conditions are valid and at least one explicit presenter intent is active:

```text
master enabled
    AND
(toggle latched OR hold pressed OR short GUI peek)
    AND
shader/mapping requirements for the selected mode are safe
```

Therefore:

- opening OBS or loading a scene never makes Spotlight appear merely because the master option was saved enabled;
- **Toggle Spotlight** latches Spotlight on/off;
- **Hold Spotlight** is true press-and-hold behavior and releases globally so a scene switch cannot leave a hidden filter stuck on;
- the OBS property API does not provide a reliable press/release button callback, so the GUI exposes a deterministic short **Spotlight Peek** while the hotkey provides true momentary behavior;
- disabling the master clears transient Toggle/Hold runtime state safely.

## 3. Shape and size contract

The primary/default shape for tutorial use is **Circle**.

Supported P5.1 appearance shapes:

- Circle — recommended/default;
- Soft Ellipse;
- Soft Rounded Rectangle.

Circle must be a true output-pixel circle (`radius_x == radius_y`) on 16:9, ultrawide, portrait, 1080p and 4K canvases.

The beginner GUI exposes one continuous **Spotlight Area Size** slider from 50% to 200%, with 100% as the default. Size remains output-space stable and must not inflate merely because camera zoom changes.

The first trial stored Ellipse and had no continuous area-size key. Absence of the new area-size key is therefore a deterministic legacy marker; those trial settings migrate once to Circle / 100% during filter creation before the first visible frame.

## 4. Mode-transition lifecycle

Mode changes are explicit runtime transitions, not passive setting changes.

A Smart/Cursor/Click transition must invalidate stale focus state and reseed from the selected mode on the next video tick:

- Smart Focus seeds from the current valid mapped presentation pointer/context;
- Cursor seeds from the current valid mapped pointer;
- Click clears the previous lock and waits for a **new** valid click after the mode transition;
- returning to Smart cannot reuse a stale Click anchor;
- activation after being off starts from current context rather than resurrecting the previous hidden visual center;
- Preview/Peek is not required to initialize or unstick any mode.

Settings/UI callbacks publish only compact transition generations. Video tick remains the owner of focus geometry.

## 5. First-click black-frame safety

Phase 4 established a critical shared-effect invariant: the Presentation Cursor sampler must always have a valid texture whenever the common GPU presentation pass may execute, even if `cursor_visible == 0`.

P5 activates the same effect pass for Spotlight. Therefore Spotlight must inherit, not bypass, the Phase 4 transparent fallback-sampler contract.

P5.1 requirements:

- when Spotlight is active and a real cursor atlas is not fully ready, force the permanent 1x1 transparent fallback sampler to be bound before the shared pass executes;
- do not assume a hidden cursor makes an unbound sampler safe; graphics backends may validate/speculatively touch declared samplers;
- click/cursor/Spotlight may share one effect pass, but no new feature may regress the fresh-install first-click safety gate;
- direct OBS clicking stress is mandatory because unit/CI tests cannot reproduce every Display Capture graphics-backend interaction.

Longer-term code cleanup should avoid duplicating shared presentation-pass ownership across feature wrappers. Until that refactor is independently proven, P5.1 favors a narrow safety hardening over destabilizing the accepted P4.1 baseline.

## 6. Performance contract

The hotfix does not authorize a heavier Spotlight architecture.

Still prohibited:

- second scene render;
- helper OBS source/scene item;
- frame readback;
- CPU mask rasterization;
- blur/bloom/particle pass;
- per-frame settings writes;
- scene-item transform mutation;
- unbounded click/pointer history.

Spotlight Off must remain on the inherited low-cost path. Toggle/Hold state is scalar/atomic. Area size and Circle coercion are analytic shader math only.

## 7. Deterministic gates

P5.1 adds/retains tests for:

- master enabled by itself does **not** request runtime Spotlight;
- Toggle/Hold/Peek each can request runtime Spotlight when master is enabled;
- master disabled blocks all runtime requests;
- an active Spotlight shared pass with a non-ready cursor requires fallback-sampler protection;
- Circle has equal X/Y radius;
- area size is monotonic from 50% → 100% → 200%;
- 200% is exactly 2× the 100% radius within tolerance;
- center remains full brightness and outer dim remains bounded;
- feather remains monotonic;
- P0–P4.1 camera/mapping/motion gates remain unchanged and green.

## 8. Required second direct-OBS acceptance

Before PR #23 can leave Draft, retest at minimum:

1. fresh OBS start with saved Spotlight master enabled: Spotlight must remain visually OFF;
2. click Display Capture repeatedly before and after Spotlight activation: **zero black/blank flicker**;
3. Toggle Spotlight button/hotkey: deterministic on/off;
4. Hold Spotlight hotkey: visible only while the hotkey is held;
5. GUI Peek: short deterministic preview, then off;
6. default/migrated shape: Circle;
7. Area Size at 50%, 100%, 150%, 200% visibly changes focus size while preserving a true circle;
8. Smart → Cursor → Click → Smart immediately after startup: each mode changes behavior without touching Preview/Peek;
9. Click mode requires a fresh click and remains content-anchored while camera pans/zooms;
10. Spotlight Off + ordinary click visualization still behaves like the accepted pre-P5 baseline.

If any first-click black frame remains, do not tune Spotlight visuals first. Treat it as P0 render safety and inspect the shared effect/sampler lifecycle before release.
