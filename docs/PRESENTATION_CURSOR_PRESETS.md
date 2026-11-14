# Presentation Cursor preset architecture

Phase 3.5 uses a preset-first UX. Normal OBS users do not import animation files, remove backgrounds, or tune hotspots.

## Runtime flow

1. User selects **Cursor Style** in the ArZoom filter.
2. A procedural built-in cursor or the bundled **Premium Hand PNG** is rendered into a transparent 28-frame RGBA atlas on preset selection. The original image is decoded once; no per-frame PNG decoding or texture upload occurs.
3. The atlas is uploaded once to the GPU; the video hot path only selects a frame index.
4. Frame 0 remains the idle pose.
5. Any click restarts frames 1..N exactly once, then returns to frame 0.
6. The cursor hotspot remains aligned with the click-ring center and the cursor scales with camera zoom.

There is no APNG/GIF decoding on the video path. Each optional PNG is decoded only when its corresponding raster preset is newly selected. The existing file loader remains available only under **Custom — Advanced**.

## Shipped ArZoom-native presets

- **Premium Hand Chrome (new default for fresh filters)** — approved glossy metallic black/chrome pointing hand, 160×160 transparent bundled PNG (5.1 KB), fingertip hotspot at (38,11), and existing 260 ms tactile press/rebound gesture.
- **Premium Hand (original)** — previous owner-approved white pointing hand (3 KB); kept as a selectable preset for backward compatibility and visual choice.
- **ArZoom Prism** — premium cyan 3D-style presentation pointer.
- **ArZoom Outline** — clean monochrome professional pointer.
- **ArZoom Azure** — restrained light-blue technical pointer.
- **ArZoom Orchid** — pastel blue/violet presentation pointer.

Earlier procedural presets remain original vector artwork. Premium Hand and Premium Hand Chrome are separately selectable owner-approved generated PNG masters, rendered by **one shared** Qt/QPainter tactile GPU-atlas pipeline. Only the active raster master is decoded; no second renderer or extra full-screen pass is introduced. No third-party cursor pack is redistributed.

## APNG strategy

Transparent APNG remains the preferred interchange/master format when future original or explicitly licensed cursor artwork is added. It preserves alpha and animation timing without background-key cleanup. For shipped built-ins, ArZoom does not depend on an APNG handler in the user's OBS installation: masters should be validated and converted to the same atlas contract before runtime.

The atlas contract is intentionally simple: frame 0 is idle; frames 1..N are one click gesture; metadata supplies frame count, duration, hotspot, and recommended base size.

## Product safety

- Fresh filters default to **Premium Hand Chrome**. Previously saved cursor selections, including Off, Classic Hand, and the original Premium Hand, are preserved; users can choose Chrome explicitly on older filters. Disable the native Display Capture cursor to avoid two pointers.
- Choosing any ArZoom preset enables Presentation Cursor immediately.
- If Display Capture still captures the native pointer, the filter warns the user to disable source cursor capture.
- Built-in hotspot metadata is fixed and hidden from the Basic UI.
- Custom import, hotspot, and background-key controls are shown only when **Custom** is selected.
- Built-in atlas generation happens only when a preset changes, never per click or per video frame.
- Any cursor-layer failure leaves Smart Camera, click visualization, and Presenter Controls functional.
