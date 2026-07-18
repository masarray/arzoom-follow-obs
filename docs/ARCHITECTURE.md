# ArZoom Architecture

## State

The hotkey changes only `requested_zoom`.

The video tick computes:

- target zoom
- target center
- smoothed zoom
- smoothed center
- monitor refresh state

The render callback only sends the final zoom and center to the GPU effect.

## State flow

```text
NORMAL
  hotkey →
ZOOMING IN
  settled →
ZOOMED / SMART FOLLOW
  hotkey →
ZOOMING OUT + RECENTER
  settled →
NORMAL / OBS PASS-THROUGH
```

A second hotkey press during an animation changes the target immediately. The animation continues from its current position without jumping.

## Coordinate systems

1. Windows virtual-desktop pixels
2. selected monitor-local pixels
3. normalized monitor coordinates `[0,1]`
4. normalized OBS texture UV coordinates `[0,1]`

Display Capture maps naturally between steps 3 and 4.

## Smart Follow

Cursor output position:

```text
output = (cursor - center) × zoom + 0.5
```

If output remains inside the safe zone, center does not move.

When output crosses a boundary, target center is moved only enough to place the cursor on that boundary.

## Edge contract

For zoom `z`:

```text
half viewport = 0.5 / z
center ∈ [half viewport, 1 - half viewport]
```

Therefore:

```text
center - half viewport >= 0
center + half viewport <= 1
```

No invalid source area can enter the frame.
