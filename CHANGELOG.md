# Changelog

## v0.6.0 — P4.1 generalized mapping + Kinematic Smart Viewport

- Promoted P4.1 Trial 8 to the accepted stable Scene Camera baseline after direct OBS validation.
- Extended safe scene-wide pointer mapping beyond fullscreen-only layouts to one visible top-level Display Capture with deterministically proven positive axis-aligned scale/inset placement and crop-aware mapping.
- Kept unsupported/ambiguous ownership fail-safe for multiple candidate Display Captures, unproven rotation/skew/flips, unsupported bounds modes, unresolved nested ownership, or invalid monitor/source geometry.
- Unified Smart Follow, click anchoring, and Presentation Cursor on the same read-only mapped coordinate path.
- Kept Presentation Cursor size tied to the exact live camera zoom.
- Split Scene Camera responsibility cleanly: `SceneViewportPlanner` decides **WHERE** to frame; `SceneKinematicMotion` decides **HOW** to move there.
- Added explicit position, velocity, and acceleration state with bounded jerk-limited integration.
- Preserved motion state through live tracking → final settle so the camera no longer performs artificial stop/restart handoffs.
- Added latched tracking, continuous follow pressure, conservative confidence-gated prediction, earlier high-zoom tracking, and bounded far-distance cruise authority.
- Added automatic moving-target cruise → immutable-target jerk-aware precision braking without resetting motion state.
- Preserved exact drift-free HOLD after final framing.
- Added deterministic P4.1 motion-quality gates for bounded jerk, no zero-speed stall, retarget velocity continuity, bounded direction reversal, tracking chatter, pointer-loss bounds, final framing, and exact HOLD.
- Locked the accepted behavior in `docs/P4_1_STABLE_BASELINE.md`; future camera work must preserve both pointer acquisition and smooth motion quality instead of trading one for the other.
- Preserved scene-safety architecture: no persistent `obs_sceneitem_set_*` mutation, no custom scene-camera input source, no duplicate/off-screen scene render graph, no CPU frame readback, and no second semantic camera planner.

## v0.5.1 — Brand identity and release media

- Adopted the supplied ArZoom artwork as the canonical product logo without AI regeneration or visual drift.
- Added PNG favicons, an Apple touch icon, a root favicon, and a six-resolution Windows ICO.
- Embedded the ArZoom icon into the Windows plugin binary and retained it across installer/uninstaller surfaces.
- Added a real OBS product screenshot and a 1200×630 Open Graph image for social sharing.
- Updated the landing page, repository presentation, structured metadata, and release validation gates.

## v0.5.0 — ArZoom Scene Camera public release

- Added **ArZoom Scene Camera**, a managed instance of the existing `arzoom_filter` attached directly to the current OBS scene source.
- Added **Tools → ArZoom — Toggle Scene Camera** and **Tools → ArZoom — Configure Scene Camera**.
- Scene Camera processes the already-composed OBS scene, so Display Capture, webcam, browser, logo, and nested-scene content can zoom together.
- Reused the accepted Smart Zone / Presenter Controls / click / Presentation Cursor runtime instead of creating a second motion engine.
- Explicitly rejected persistent `obs_sceneitem_set_*` scene transform mutation; ArZoom does not write scene-item position, scale, rotation, bounds, or crop.
- Removed the early P4 custom-camera-source / off-screen `gs_texrender_t` experiment in favor of OBS-native scene filter semantics.
- Added deterministic Scene Camera identity and toggle-policy gates.
- Added conservative Display Capture → scene mapping: exactly one visible fullscreen Display Capture with a deterministically resolved monitor is required for pointer-driven scene-wide Smart Follow/click/cursor mapping.
- Ambiguous inset/scaled/rotated/multiple Display Capture layouts fail safe instead of guessing cursor coordinates.
- Added warnings for nested/per-source ArZoom filters that can cause double zoom and for native Display Capture cursor capture when Presentation Cursor is active.
- Fixed a fresh-install **first-click black-frame/flicker** path by binding a permanent transparent fallback cursor sampler before the shared GPU effect can be activated without a real cursor atlas.
- Added deterministic render-safety tests covering disabled/not-ready/missing-atlas cursor states.
- Added a branded multi-resolution Windows installer icon and current v0.5.0 installer/version metadata.
- Updated installer/manual package guidance, README, release notes, and public website for the Scene Camera workflow.
- Windows packaging continues to publish an installer, manual ZIP, and SHA-256 checksums.

## v0.4.1 — Presentation Cursor

- Added preset-first Presentation Cursor UI.
- Added seven original ArZoom-native cursor styles: Prism, Outline, Azure, Orchid, Parakeet, Classic Hand, and Sticker Hand.
- Added short tactile click micro-interactions with exact return to frame 0.
- Added arrow-tip/fingertip hotspot ownership and cursor scaling with camera zoom.
- Built-ins render to a transparent GPU atlas only when preset/settings change; no image decoding occurs on click or per video frame.
- Kept Advanced custom GIF/WebP/PNG assets.
- Added double-cursor warning when Display Capture still captures its native cursor.
- Added deterministic playback, hotspot, zoom-scaling, Overview Peek, and Smart Camera isolation gates.

## v0.4.0 — Presenter Controls

- Added Toggle Zoom, Hold Zoom, Freeze Camera, Toggle Smart Follow, Zoom In/Out, Reset / Full Frame, and Overview Peek.
- Overview Peek holds a smooth exact 1× overview and restores the exact saved affine shot on release.
- Added deterministic multi-filter target semantics and profile-persistent OBS hotkeys.
- Kept presenter-control state bounded and isolated from Smart Zone camera math.

## v0.3.1 — Premium dual-vector click visualization

- Replaced experimental liquid/blob visuals with clean dual analytic vector rings.
- Left click: Azure + Aqua; right click: Violet + Orchid; middle click: compact Amber + Gold.
- Added quintic minimum-jerk expansion, staggered second ring, clean dissolve, and luminance-aware support on bright/dark content.
- Preserved one-pass GPU rendering, fixed four-slot allocation-free click state, content anchoring, and Smart Camera isolation.

## v0.3.0 — GPU Click Visualization public trial

- Added procedural GPU click feedback to the existing presentation pass.
- Click events are stored in normalized content coordinates and remain anchored while zoom/pan moves.
- Added deterministic click capacity/expiry/anchoring/isolation gates.
- No PNG generation, particle system, extra OBS image source, CPU rasterization, frame readback, or separate bloom pass.

## v0.2.0 — Smart Zone Gimbal Camera

- Replaced the early edge-triggered follower with the shared platform-neutral Smart Camera core.
- Added focus-preserving zoom activation and straight affine minimum-jerk zoom-in/zoom-out.
- Added Smart Zone presentation-area semantics, SmoothIdle, Coast handoff, hysteresis, soft wake-up, continuous retargeting, edge safety, and exact settle.
- Added Cinematic, Balanced, and Responsive camera characters.
- Added deterministic closeout gates across common frame rates and edge/corner stress cases.
- Added the Standard OBS / OBS Portable-aware Windows installer.

## v0.1.4 — Persistent hotkey and beginner setup

- Explicitly save/restore the global ArZoom frontend hotkey across OBS restarts and profile changes.
- Added Open OBS Hotkeys Settings from the filter UI.
- Added visible hotkey configuration status.

## v0.1.3 — Global OBS hotkey visibility

- Replaced the per-filter source hotkey with one module-level frontend hotkey.
- Added multiple-filter target semantics for enabled/showing ArZoom filters.

## v0.1.2 — Runtime creation and shader compatibility

- Fixed the blank “No properties available” filter panel.
- Replaced the effect interface with an OBS-compatible vertex/fragment contract.
- Added fail-safe pass-through behavior for graphics-resource failure.

## v0.1.1 — Windows packaging fixes

- Fixed OBS plugin-template install layout packaging.
- Added package-only mode and cached fast-build behavior.
- Added ZIP payload verification.

## v0.1.0 — Initial native Windows MVP

- OBS hotkey-driven zoom and mouse follow.
- Safe-zone following, smooth frame-rate-independent zoom/pan, hard viewport clamping, monitor mapping, multi-monitor support, idle pass-through, fail-safe rendering, Windows build/installer, English/Indonesian locale, and deterministic edge math tests.
