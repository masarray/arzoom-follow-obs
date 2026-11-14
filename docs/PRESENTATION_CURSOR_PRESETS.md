# Presentation Cursor preset architecture

Phase 3.5 uses a preset-first UX. Normal OBS users do not import animation files, remove backgrounds, or tune hotspots.

## Runtime flow

1. User selects **Cursor Style** in the ArZoom filter.
2. A procedural built-in cursor or a bundled **transparent PNG** is rendered into a transparent 28-frame RGBA atlas on preset selection. The original image is decoded once; no per-frame PNG decoding or texture upload occurs.
3. The atlas is uploaded once to the GPU; the video hot path only selects a frame index.
4. Frame 0 remains the idle pose.
5. Any click restarts frames 1..N exactly once, then returns to frame 0.
6. The cursor hotspot remains aligned with the click-ring center and the cursor scales with camera zoom.

There is no APNG/GIF decoding on the video path. Each optional PNG is decoded only when its corresponding raster preset is newly selected. The existing file loader remains available only under **Custom — Advanced**.

## Shipped ArZoom-native presets

- **3D Hand Pointer (optional choice)** — user-supplied 3D hand illustration, transparent 160×160 optimized PNG (~5.2 KiB), fingertip hotspot (14,15); designed for a readable 64–96 px presentation pointer. Reuses the same 28-frame/260 ms raster atlas without changing the default. Before public redistribution, confirm that the source artwork may be bundled under the project's distribution terms.
- **Premium Chrome Arrow (optional choice)** — redrawn smooth-contour metallic pointer, transparent 160×160 PNG (3.7 KB), calibrated tip hotspot (25,4). Verified at 32/48/64 px on dark and light backgrounds; retains existing 260 ms tactile gesture. No default or existing saved selection is changed.
- **Premium Hand Chrome (new default for fresh filters)** — approved glossy metallic black/chrome pointing hand, 160×160 transparent bundled PNG (5.1 KB), fingertip hotspot at (38,11), and existing 260 ms tactile press/rebound gesture.
- **Premium Hand (original)** — previous owner-approved white pointing hand (3 KB); kept as a selectable preset for backward compatibility and visual choice.
- **ArZoom Prism** — premium cyan 3D-style presentation pointer.
- **ArZoom Outline** — clean monochrome professional pointer.
- **ArZoom Azure** — restrained light-blue technical pointer.
- **ArZoom Orchid** — pastel blue/violet presentation pointer.

Earlier procedural presets remain original vector artwork. Premium Hand, Premium Hand Chrome, Premium Chrome Arrow and 3D Hand Pointer are separately selectable owner-approved generated PNG masters, rendered by **one shared** Qt/QPainter tactile GPU-atlas pipeline. Only the active raster master is decoded; no second renderer or extra full-screen pass is introduced. The 3D Hand artwork retains its original pose and is optimized to an indexed PNG with transparency. No third-party cursor pack is redistributed.

## APNG strategy

Transparent APNG remains the preferred interchange/master format when future original or explicitly licensed cursor artwork is added. It preserves alpha and animation timing without background-key cleanup. For shipped built-ins, ArZoom does not depend on an APNG handler in the user's OBS installation: masters should be validated and converted to the same atlas contract before runtime.

The atlas contract is intentionally simple: frame 0 is idle; frames 1..N are one click gesture; metadata supplies frame count, duration, hotspot, and recommended base size.

## Product safety

- Fresh filters default to **Premium Hand Chrome**. Previously saved cursor selections, including Off, Classic Hand, and the original Premium Hand, are preserved; users can choose Chrome Hand or Chrome Arrow explicitly on older filters. Disable the native Display Capture cursor to avoid two pointers.
- Choosing any ArZoom preset enables Presentation Cursor immediately.
- If Display Capture still captures the native pointer, the filter warns the user to disable source cursor capture.
- Built-in hotspot metadata is fixed and hidden from the Basic UI.
- Custom import, hotspot, and background-key controls are shown only when **Custom** is selected.
- Built-in atlas generation happens only when a preset changes, never per click or per video frame.
- Any cursor-layer failure leaves Smart Camera, click visualization, and Presenter Controls functional.

## Premium Chrome Arrow artwork quality gate (2026-11-15)

The first image-derived sprite contained fine reflection noise and edge artifacts that became distracting at normal cursor size. Byte-level PNG integrity alone did not detect this. The replacement was rebuilt with a closed vector silhouette, a solid readable black contour, a restrained chrome rim, and a clean white interior, then downsampled and checked at **32, 48, and 64 px on both dark and light backgrounds**. The new image has one connected alpha silhouette without detached speckles. The editable art master is `assets/source/premium-chrome-arrow.svg`; only the optimized PNG is installed with OBS.

The raster preset, 28-frame tactile atlas, shader pass and saved-style IDs remain unchanged. Physical OBS testing is still required to confirm the fingertip/arrow hotspot against the click ring.
