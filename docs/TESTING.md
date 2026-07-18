# ArZoom Runtime Test Plan

## Functional

- Toggle zoom 100 times.
- Press the hotkey again during zoom-in.
- Hold the hotkey and confirm there is no repeated toggle.
- Move the cursor rapidly to every corner.
- Move the cursor outside the captured monitor.
- Change zoom amount while zoomed.
- Hide and show the source.
- Switch scenes during zoom.
- Start and stop recording during zoom.
- Start and stop streaming during zoom.

## Displays

- single 1080p monitor
- dual monitor with second display left of primary
- 100% + 150% mixed DPI
- portrait display
- two displays with identical resolution
- non-primary captured monitor

## Performance

- 1080p60 streaming
- 1080p60 streaming and recording together
- 1440p60
- 4K30
- 4K60

Observe:

- rendering lag
- skipped frames
- encoder overload
- memory growth
- visible jitter
- black edges
- source flicker
- OBS crash or deadlock

## Pass criteria

- no black frames
- no exposed area outside the monitor
- no cumulative memory growth
- no state corruption after repeated hotkeys
- no visible idle overhead
- smooth movement that remains comfortable during long tutorials
