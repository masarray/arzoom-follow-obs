# Presentation Cursor preset architecture

Phase 3.5 uses a preset-first UX. Normal OBS users do not import animation files, remove backgrounds, or tune hotspots.

## Runtime flow

1. User selects **Cursor Style** in the ArZoom filter.
2. A built-in ArZoom-native cursor is generated as a transparent 28-frame RGBA atlas only when the preset changes.
3. The atlas is uploaded once to the GPU; the video hot path only selects a frame index.
4. Frame 0 remains the idle pose.
5. Any click restarts frames 1..N exactly once, then returns to frame 0.
6. The cursor hotspot remains aligned with the click-ring center and the cursor scales with camera zoom.

There is no APNG/GIF decoding for built-in presets at runtime. The existing file loader remains available only under **Custom — Advanced**.

## Shipped ArZoom-native presets

- **ArZoom Prism** — premium cyan 3D-style presentation pointer.
- **ArZoom Outline** — clean monochrome professional pointer.
- **ArZoom Azure** — restrained light-blue technical pointer.
- **ArZoom Orchid** — pastel blue/violet presentation pointer.

All built-ins are original ArZoom vector geometry rendered with Qt/QPainter. Third-party cursor packs used as visual references during prototyping are not committed or redistributed.

## APNG strategy

Transparent APNG remains the preferred interchange/master format when future original or explicitly licensed cursor artwork is added. It preserves alpha and animation timing without background-key cleanup. For shipped built-ins, ArZoom does not depend on an APNG handler in the user's OBS installation: masters should be validated and converted to the same atlas contract before runtime.

The atlas contract is intentionally simple: frame 0 is idle; frames 1..N are one click gesture; metadata supplies frame count, duration, hotspot, and recommended base size.

## Product safety

- Default style remains **Off / captured system cursor** to avoid silently creating a double cursor on existing scenes.
- Choosing any ArZoom preset enables Presentation Cursor immediately.
- If Display Capture still captures the native pointer, the filter warns the user to disable source cursor capture.
- Built-in hotspot metadata is fixed and hidden from the Basic UI.
- Custom import, hotspot, and background-key controls are shown only when **Custom** is selected.
- Built-in atlas generation happens only when a preset changes, never per click or per video frame.
- Any cursor-layer failure leaves Smart Camera, click visualization, and Presenter Controls functional.
