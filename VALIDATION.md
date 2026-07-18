# Validation Record — v0.1.4 source

Completed in the source-generation environment:

- C++17 motion-core compile and unit tests: PASS
- module lifecycle audit: global hotkey is registered from `obs_module_load()` and unregistered from `obs_module_unload()`: PASS
- hotkey API audit: exactly one `obs_hotkey_register_frontend()` call and no `obs_hotkey_register_source()` call: PASS
- filter registry add/remove lifecycle audit: PASS
- anti-repeat atomic state audit: PASS
- global toggle selection logic audit: showing enabled filters first, all enabled filters as fallback: PASS
- buildspec and CMake preset JSON parse: PASS
- GitHub Actions YAML parse: PASS
- source package integrity check: PASS

The v0.1.4 runtime contract is intentionally fail-safe:

- the OBS hotkey row exists independently of filter instance creation;
- a missing or invalid effect never creates a null/ghost filter;
- filter properties and the global OBS hotkey remain available;
- video stays in pass-through mode until the effect is valid;
- DLL and effect use ABI-v2 uniform names to reject stale mixed payloads.

Still required on Windows before stable release:

- MSVC build against the cached OBS SDK
- loading the rebuilt DLL in OBS Studio
- confirming the hotkey row in OBS Settings
- direct 1080p60 recording and streaming test
- mixed-DPI physical monitor test
- NVIDIA/AMD/Intel runtime verification

## Hotkey persistence contract

- The frontend hotkey is registered once under `arzoom.toggle`.
- The saved JSON wrapper in the active OBS profile is loaded explicitly after registration.
- The binding is saved before profile changes and on OBS exit/module unload.
- The filter panel can open OBS Settings directly on the Hotkeys page.
- Older CMake caches with frontend/Qt disabled are automatically reconfigured.
