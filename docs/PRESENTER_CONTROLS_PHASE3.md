# ArZoom Phase 3 — Presenter Controls

Phase 3 turns the frozen v0.3.1 Smart Zone camera into a presenter-operated camera system without changing the accepted Smart Camera motion or premium dual-vector click visuals.

## Control model

ArZoom exposes eight frontend hotkeys in OBS:

| Control | Behavior |
| --- | --- |
| Toggle Smart Camera Zoom | Existing latched zoom toggle; existing `arzoom.toggle` profile binding remains compatible. |
| Hold Zoom | Zoom while held. Releasing it does not cancel a latched Toggle Zoom. |
| Freeze / Unfreeze Camera | Holds the exact current zoom + center. Pointer movement cannot change framing until unfrozen. |
| Toggle Smart Follow | Suspends/resumes pointer-driven Smart Zone movement without turning zoom off. |
| Increase Zoom | Adds 0.25x, clamped to 4.00x. |
| Decrease Zoom | Subtracts 0.25x, clamped to 1.10x. Edge-safe framing may temporarily limit zoom-out until framing can remain valid. |
| Reset / Full Frame | Clears active zoom/hold/freeze/peek and returns safely to 1x. Smart Follow preference is preserved. |
| Hold Overview Peek | Temporarily shows exact 1x overview; release returns to the exact pre-peek shot. |

## Overview Peek

Overview Peek is intentionally not implemented as a second Smart Camera mode.

```text
focused shot
    ↓ HOLD
minimum-jerk affine transition
    ↓
exact 1x full-frame overview
    ↓ RELEASE
minimum-jerk affine transition
    ↓
exact saved focus + zoom
```

The Smart Zone camera state is paused while Overview Peek owns the visible transform. Pointer movement during the peek therefore cannot move the saved shot.

Both directions interpolate screen `scale + offset` with the same quintic minimum-jerk scalar used by the Phase 1 straight-path zoom contract.

## Freeze vs Follow Off

These controls intentionally solve different presenter problems.

**Freeze Camera** means *do not change the shot*. Both center and visible zoom stay exact. If the presenter changes the configured zoom while frozen, that target is used only after unfreezing.

**Smart Follow Off** means *do not follow the pointer*. The viewport center stays stable, but live Zoom +/- remains available through the existing edge-safe active-zoom smoothing.

When Smart Follow is re-enabled, the video thread seeds the old presentation-area cursor anchor for one frame before exposing the current cursor. This makes a remote pointer location enter the normal Smart Zone intent path instead of becoming an immediate camera target.

## Multiple-filter targeting

Every presenter command uses one rule:

1. If enabled ArZoom filters are currently showing, act on that showing set.
2. If none are showing, act on all enabled ArZoom filters as a safe fallback.
3. Momentary key releases (Hold Zoom and Overview Peek) clear all registered instances so a scene switch while a key is held cannot leave a hidden filter stuck.

## Threading / lifecycle contract

- Hotkey callbacks perform bounded atomic/scalar state changes only.
- Monitor geometry, current camera fields and Smart Follow resume anchors are owned/read by the video tick, not the hotkey thread.
- Zoom-setting persistence is queued onto the Qt/main-window event loop; the hotkey callback never blocks on source-setting writes.
- Queued setting updates verify the filter is still registered before dereferencing it.
- No extra render pass, frame readback, particle system or per-frame allocation is introduced.

## Deterministic gates

Phase 3 adds gates for:

- Hold Zoom + latched Toggle composition;
- 0.25x bounded Zoom +/-;
- exact Overview Peek 1x hold;
- exact return to the saved affine transform;
- straight screen-space overview trajectory;
- cursor-independent saved-shot geometry;
- Reset cancellation to exact full frame;
- consistent overview endpoints at 30 / 60 / 120 / 144 fps.

All Phase 0, Phase 1 and Phase 2 gates remain mandatory.

## Direct OBS trial matrix

Before merging v0.4.0, test these visually:

1. **Toggle Zoom only** — existing v0.3.1 behavior must feel unchanged.
2. **Hold Zoom with Toggle OFF** — press zooms; release returns smoothly.
3. **Hold Zoom with Toggle ON** — release must remain zoomed.
4. **Freeze in SmoothIdle** — move pointer across the screen; framing must remain pixel-stable.
5. **Freeze mid-follow / mid-activation** — shot freezes exactly; unfreeze must not jump.
6. **Smart Follow OFF while zoomed** — pointer can move anywhere without moving the viewport; Zoom +/- still works.
7. **Smart Follow ON after a remote pointer move** — reacquisition must start softly through Smart Zone intent.
8. **Overview Peek at 2x/3x/4x and near corners** — hold reaches exact full frame with no curved detour; release restores the exact previous shot.
9. **Move pointer during Overview Peek** — release must still restore the original shot, not the new pointer area.
10. **Reset during Overview Peek** — remain on the safe full-frame path; do not restore the old shot.
11. **Rapid hotkeys** — no stuck hold/freeze/peek state.
12. **Scene switch while Hold Zoom / Overview Peek is held** — releasing the key must clear every previously targeted instance.
13. **Multiple ArZoom filters** — showing enabled filters receive commands; fallback behavior remains deterministic when none are showing.
14. **Click visual during controls** — accepted v0.3.1 Azure/Aqua and Violet/Orchid feedback remains content-anchored and does not affect camera state.

## Phase 3 exit gate

Phase 3 remains a draft/public-trial candidate until:

- deterministic Phase 0/1/2/3 gates are green;
- Windows MSVC plugin + Inno installer package successfully;
- direct OBS trial confirms Freeze, Follow toggle, Hold Zoom and Overview Peek are comfortable and predictable;
- no regression is observed in v0.3.1 Smart Zone motion or click feedback.
