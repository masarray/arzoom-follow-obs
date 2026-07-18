# Changelog


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
- Added ABI-v2 uniform names to prevent mixed old DLL/effect packages from partially running.
- Uses texture-backed filter processing for reliable Display Capture input.
- Shows a clear ready/error status inside the filter instead of creating a ghost instance.
- Keeps fail-safe pass-through behavior on every graphics-resource failure.

## v0.1.1

- Fixed Windows packaging for the official OBS template install layout (`arzoom/bin/64bit` and `arzoom/data`).
- Added `package-existing-build.bat` so a successful compile can be packaged without rebuilding OBS.
- Added cached fast-build behavior; existing template dependencies and CMake configuration are reused by default.
- Added explicit `-PackageOnly`, `-RefreshTemplate`, and `-Reconfigure` build options.
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
