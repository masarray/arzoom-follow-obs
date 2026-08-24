# Changelog

## v0.3.1 — Premium dual-vector click candidate

- Replaced the rejected water-dimple/blob experiment with a clean **dual analytic vector-ring** interaction designed for professional screen recording and teaching.
- Left click now uses two thin staggered circles in a modern Azure + Aqua palette; right click uses the same visual language in Violet + Orchid so intent is distinguishable without labels or symbols.
- Both rings expand with quintic minimum-jerk timing, giving soft launch and soft finish instead of linear pop/stop motion.
- The second ring starts roughly 45–60 ms after the first, creating a deliberate premium cadence rather than a noisy particle burst.
- Removed all click-time texture distortion, refraction, angular wobble, blobs, liquid noise, and thick neon-ring emphasis. The captured frame stays optically clean.
- Rings are generated analytically in the GPU shader from distance fields, so they remain perfectly circular, symmetric, sharp, resolution-independent, and require no PNG/vector asset files.
- Added luminance-aware compositing with a subtle chromatic under-stroke on bright/white surfaces, keeping clicks visible in Explorer and document UIs without clipping cyan/violet to white.
- Glow is intentionally restrained and primarily visible on dark content; the exact vector line remains the visual hero.
- Retuned fixed click lifetimes to 0.44 s left / 0.50 s right / 0.32 s middle so the effect is noticeable but disappears quickly during rapid tutorials.
- Preserved the one-pass GPU architecture, fixed four-slot allocation-free CPU state, content-space anchoring, and frozen Smart Zone camera contract.
- No global mouse hook is added in this patch. Explorer input capture remains isolated from the visual/compositing issue; the input layer will only be replaced if real missed click events remain after visibility is confirmed.

## v0.3.0 — GPU Click Visualization public trial

- Added procedural GPU click feedback to the existing ArZoom presentation pass; no PNG generation, temporary files, particle system, extra OBS image source, frame readback, or separate bloom pass.
- Left click uses a compact liquid-like cyan expanding ring with subtle analytic deformation and soft glow.
- Right click uses a visually distinct violet dual/delayed ring so it is recognizable without text labels.
- Middle click uses a compact gold pulse.
- Click events are stored in normalized source/content coordinates and reprojected through the live camera transform every frame, so feedback stays attached to the clicked content while zoom/pan moves.
- Added a fixed four-slot, allocation-free click event core with deterministic expiry and bounded overwrite behavior.
- Windows click capture uses compact mouse-button edge sampling and records only clicks inside the mapped Display Capture monitor.
- Click visualization remains independent of Smart Zone intent: click events do not set camera emphasis, urgency, wake SmoothIdle, or retarget Follow by default.
- Rendering remains one filter pass whether zoom, click feedback, or both are visible; when neither is visible the OBS pass-through path remains active.
- Added a simple **Show click visualization** toggle without exposing shader-engineering controls in the Basic UI.
- Added deterministic Phase 2 gates for click ordering/capacity, expiry, content anchoring under 1x/2x/3x/4x camera transforms, finite edge/corner projection, and camera-output isolation.
- Retained the complete Phase 0 + Phase 1 regression and closeout matrix unchanged.
- Added Phase 2 microbenchmarks; hosted Windows diagnostics are approximately 7 ns/update for idle/one-click state and 9 ns/update for four overlapping clicks. Absolute timings are runner-specific engineering diagnostics.

## v0.2.0 — Smart Zone Gimbal Camera + portable-aware installer

- Replaced the v0.1.x edge-triggered follower with a shared platform-neutral Smart Camera core.
- Rejected the early ballistic/spring experiment after visual trial showed follow snapping and zoom-out hunting; the final camera is a super-steady gimbal model.
- Added focus-preserving zoom activation so edge/corner zoom goes directly toward the intended subject instead of detouring through unrelated center content.
- Added affine-transform zoom-in and zoom-out with quintic minimum-jerk easing; fixed source pixels follow straight screen-space paths with soft start and soft finish.
- Added **Smart Zone** semantics: ArZoom follows presentation-area changes rather than ordinary local mouse movement.
- Added **SmoothIdle**: circles, repeated pointing, jitter, and explanatory cursor movement inside the current area keep the viewport exact-stable without waiting for the mouse to stop.
- Added **Coast** handoff so Follow does not snap to steady; live-pointer influence fades while camera speed decays naturally before exact idle lock.
- Added inner/outer zone hysteresis and short relocation dwell so boundary movement does not chatter between Follow and Idle.
- Leaving the outer Smart Zone starts a new follow with a soft first movement step; edge risk and semantic emphasis may shorten the delay without changing physics.
- Preserved continuous retargeting during real travel: moving the mouse again changes only the destination while gimbal filter state remains continuous.
- Removed default predictive look-ahead to prioritize stability and avoid unnecessary correction movement.
- Kept edge urgency, but it only shortens the same gimbal time constants through a filtered urgency value; there is no alternate high-energy motion mode.
- Added deterministic closeout gates for straight zoom paths, local explanation lock, Follow → Coast → SmoothIdle, soft idle wake-up, continuous retargeting, rapid zone switching, 2x/3x/4x corner zoom-out, and 30/60/120/144 fps behavior.
- Retained Phase 0 randomized edge/math invariants and benchmark gates.
- Motion styles are presented as Cinematic, Balanced, and Responsive while preserving old persisted setting values for profile compatibility.
- Added a fool-proof Windows installer mode selector for Standard OBS Studio or OBS Portable/custom folders.
- Standard mode auto-detects common OBS install locations; portable/custom mode lets the user browse to a specific OBS root.
- Installer validates `bin\\64bit\\obs64.exe` before copying plugin files, remembers the last valid custom root, and launches the selected OBS installation after setup.
- Windows CI compiles the actual Inno Setup installer in addition to the manual ZIP.

## Unreleased — Public repository and website

- Rebuilt the public README around the user problem, download flow, five-minute setup, recommended defaults, compatibility, privacy, troubleshooting, and honest preview status.
- Added a responsive GitHub Pages product website with a landing page, beginner guide, troubleshooting guide, privacy overview, branded 404 page, sitemap, robots directives, structured data, and accessible reduced-motion behavior.
- Added automated static-site deployment from `docs/` through GitHub Actions.
- Added public support, contribution, security, and community conduct policies.
- Added structured bug and feature request forms for actionable OBS, GPU, monitor, DPI, and reproduction data.
- Added automatic Windows release publishing from `buildspec.json`.
- Added the stable release asset name `ArZoom-OBS-Setup-windows-x64.exe` so every public Download button can point directly to the latest installer.
- Added a clearly named manual-install ZIP and SHA-256 checksums for advanced users.

## v0.1.4 — Persistent hotkey and beginner-friendly setup

- Restore the ArZoom frontend hotkey explicitly from the active OBS profile at startup.
- Save the binding on profile changes, OBS exit, module unload, and on-demand from the filter panel.
- Clear or reload bindings correctly when the active OBS profile changes.
- Add **Open OBS Hotkeys Settings** directly in the filter properties and jump to the Hotkeys page.
- Add visible configured/not-configured hotkey status in the filter panel.
- Enable the required OBS frontend API and Qt Widgets dependencies.
- Automatically reconfigure an older cached build when frontend/Qt support was previously disabled.

## Repository hygiene

- Hardened `.gitignore` against OBS template caches, CMake/MSBuild output, staging/release packages, installers, archives, patch files, and generated workflow/build Markdown reports.
- Added `untrack-ignored-files.bat` to remove previously tracked generated files from the Git index without deleting local build caches.

## v0.1.3 — Global OBS hotkey visibility fix

- Replaced the per-filter source hotkey with one module-level frontend hotkey.
- The row `ArZoom — Toggle Zoom & Mouse Follow` is now registered as soon as OBS loads the plugin, so it appears in Settings → Hotkeys even before a filter instance is created.
- The global hotkey toggles all currently showing and enabled ArZoom filters in sync; if none are showing, it toggles all enabled instances.
- Removed the ineffective per-filter hotkey save/load path.
- Kept anti-repeat behavior and atomic filter state updates.
- Added explicit load log: `[ArZoom] Global frontend hotkey registered`.

## v0.1.2 — Runtime creation and shader compatibility fix

- Fixed the blank “No properties available” filter panel.
- Registers properties and the per-source hotkey even when the GPU effect cannot load.
- Replaced the effect interface with an OBS-reference-compatible vertex/fragment contract.
- Added ABI-v2 uniform names to prevent mixed old DLL/new effect packages from partially running.
- Uses texture-backed filter processing for reliable Display Capture input.
- Shows a clear ready/error status inside the filter instead of creating a ghost instance.
- Keeps fail-safe pass-through behavior on every graphics-resource failure.

## v0.1.1

- Fixed Windows packaging for the official OBS template install layout (`arzoom/bin/64bit` and `arzoom/data`).
- Added `package-existing-build.bat` so a successful compile can be packaged without rebuilding OBS.
- Added cached fast-build behavior; existing template dependencies and CMake configuration are reused by default.
- Added explicit `-PackageOnly`, `-RefreshTemplate`, and `-Reconfigure` options.
- Added ZIP payload verification before reporting packaging success.

## v0.1.0

Initial native Windows MVP source:

- one OBS hotkey toggles zoom and mouse follow
- Smart Follow safe zone for comfortable viewing
- smooth, frame-rate-independent zoom and pan
- hard viewport edge clamping
- centered and fixed zoom modes
- automatic OBS Display Capture monitor mapping
- manual monitor fallback
- multi-monitor negative-coordinate support
- idle OBS pass-through
- fail-safe shader and mapping behavior
- Windows local build, ZIP packaging, installer, and GitHub Actions
- English and Indonesian locale
- deterministic motion and 200,000-case edge invariant test
