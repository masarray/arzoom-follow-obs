# ArZoom Premium Presentation vNext — Architecture & Implementation Design (ARD)

**Status:** PROPOSED / design decision record (no runtime change in this docs PR)  
**Date:** 2026-11-14  
**Companion PRD:** [ArZoom Premium Presentation PRD](ARZOOM_PREMIUM_PRESENTATION_PRD.md)  
**Reference code:** [ArZoom main](https://github.com/masarray/arzoom-follow-obs), [Arssyut main](https://github.com/masarray/arssyut), [ArZoom Multi Draft #34](https://github.com/masarray/arzoom-follow-obs/pull/34)

## 1. Executive architecture decisions

| ID | Decision | Reason |
| --- | --- | --- |
| ADR-01 | Retain ArZoom's accepted camera: Single WHERE = `SceneViewportPlanner`, HOW = `SceneKinematicMotion` | Arssyut's `ArZoomCameraAdapter` uses the ArZoom `PresenterAwareSmartCamera` in per-source mode; not a proven replacement scene camera |
| ADR-02 | Port Arssyut **P4R.3 single-ring visual algorithm**, not the D3D11 compositor | OBS owns graphics device/context/effect; match the appearance in existing OBS Draw shader |
| ADR-03 | Preserve **one shared presentation pass**, a fixed four-click array, and one camera transform | No extra full-frame pass, no particle system or competing projection |
| ADR-04 | Port Arssyut's **portable shortcut semantic reducer + physical keycap model** | Tested privacy/geometry behavior; bounded five-keycap frame |
| ADR-05 | Implement **one opt-in module-owned Windows input observer**, not Arssyut Raw Input registration inside OBS | Windows explicitly warns library Raw Input registrations can steal the host's per-class window target |
| ADR-06 | Render keycaps as a retained native CPU raster/OBS dynamic texture sampled in the existing Draw effect | No per-frame glyph raster/texture creation and no D3D11 device ownership collision |
| ADR-07 | Any optional click-triggered auto-zoom is an **intent request** to the existing planner, not a second camera | Preserve manual controls, freeze, overview, fail-safe mapping and settled HOLD |
| ADR-08 | Keep ArZoom Single frozen baseline and integrate Multi only at its M5 canonical pointer gate | Draft PR #34's M0/M1/M2A is not yet approved for camera/render wiring |
| ADR-09 | No `arzoom-filter-v25.cpp` include-wrapper patch for new features | The existing v2→v24 phase stack needs explicit bounded integration seams, not more nested override state |
| ADR-10 | Shader parameter publication is one explicit, complete, neutralized frame contract | Previous P5 D3D11 shader ABI omissions caused black output when Spotlight was Off |

## 2. Actual baseline and protected invariants

As audited 2026-11-14:

- `CMakeLists.txt` builds `src/arzoom-filter-v24.cpp`, which includes `v23.cpp` and earlier phased implementation. `src/plugin-main.cpp` registers `arzoom_filter_info`.
- `src/arzoom-filter-v3.cpp` owns `Phase2Filter::clicks`, samples button edges with `GetAsyncKeyState` in `phase2_tick`, and maps click into the current source/monitor coordinate chain. This is **not** the same input architecture as Arssyut's recorder Raw Input worker.
- `src/arzoom-click-visual.hpp` provides four fixed slots, current lifetimes 0.32–0.50 s and content-to-output projection; Arssyut vendors the identical header SHA but implements a different controller and shader skin.
- `data/effects/arzoom.effect` contains the current dual rings, spotlight, presentation cursor and the shared `Draw` / `PSZoom` pass. Rendering/cursor/Spotlight paths are coupled through GPU ABI.
- Arssyut `presentation_controller.cpp` coalesces fast same-target clicks, consumes click intent for an auto-zoom window; its compositor shader generates the accepted P4R.3 single emissive ring.
- Arssyut `shortcut_visualizer.cpp` returns a fixed `KeyboardOverlayFrame`; `d3d11_compositor.cpp` rasterizes physical keycaps to retained GDI resources and uploads only on generation changes.
- The accepted scene camera, OBS scene composition, non-mutating mapping, cursor hotspots, Spotlight choreography, and P0 render fallback are regression-locked.

No OBS scene-item transform writes, no duplicate scene rendering, no CPU output frame readback, no per-tick filesystem/texture allocation, no per-frame glyph raster, no blocking render/input callbacks.

## 3. Target boundary diagram

~~~text
                    OBS Source/Scene + Presenter Hotkeys
                                    |
                           Existing camera intent
                                    |
                    Canonical Single / Multi mapping
                                    |
                          ONE canonical pointer
                                    |
                      SceneViewportPlanner (WHERE)
                                    |
                     SceneKinematicMotion (HOW)
                                    |
                       Authoritative camera output
                                    |
       +----------------------------+-----------------------------+
       |                            |                             |
   Click event                 Spotlight/cursor            Keycap chord snapshot
   existing sampler            existing layers            opt-in observer service
       |                            |                             |
   ClickVisualReducer         read-only camera            ShortcutPolicyReducer
   4 bounded slots            presentation state          <= 5 keycaps; generation
       |                            |                             |
       +------------------------+---+-----------------------------+
                                |
                   PresentationOverlaySnapshot
                   typed, compact, neutral defaults
                                |
               Existing OBS filter Draw / PSZoom pass
                camera + spotlight + click + cursor
                     + sampled keycap atlas
                                |
                          One OBS output
~~~

**Ownership:** camera is the sole authority for pan/zoom; the overlay subsystem can only read its transform. Keycap placement is final output-space, not content-space; click center is content-anchored then projected once. No renderer may resolve a different monitor/source mapping.

## 4. Core modules and source integration

Suggested implementation modules (final naming may adapt to the repository without adding more phase wrappers):

| Module | Proposed location | Single responsibility |
| --- | --- | --- |
| `ClickVisualReducer` / style specification | `src/presentation/arzoom-click-visual-v2.hpp` (or extend existing validated header) | Fixed slots, same-target retrigger, age→normalized progress, style constants, no OBS headers |
| `ShortcutChord`, `ShortcutVisualPolicy` | `src/presentation/arzoom-shortcuts.hpp/.cpp` | Portable key/modifier normalization and privacy guard; maximum five structured physical keys |
| `PresentationInputService` | `src/platform/windows/arzoom-keyboard-observer.*` | One module-owned optional keyboard hook, bounded semantic event publication, lifecycle and status |
| `KeycapRasterizer` | `src/presentation/arzoom-keycap-raster.*` | Retained GDI or equivalent glyph raster cache; generation-only CPU updates; no Win32 paint in video/render callbacks |
| `KeycapGpuAtlas` | `src/presentation/arzoom-keycap-texture.*` | Graphics-thread creation/upload/release of one retained OBS texture, fixed max dimensions, known neutral texture |
| `PresentationOverlaySnapshot` | `src/presentation/arzoom-overlay-state.hpp` | Immutable typed event/style/snapshot handoff to existing shared renderer |
| OBS adapter | Existing `src/arzoom-filter-v3.cpp` click tick/integration, final render ownership `src/arzoom-filter-v18.cpp`/`v21.cpp`/`v24.cpp` after exact call-chain review | Update a **single existing render ownership site**, not an additional wrapper |
| OBS shader | `data/effects/arzoom.effect` | OBS HLSL-compatible emissive single-ring and bounded keycap sampling in `Draw` |
| OBS UI + locale | Existing filter property chain and `data/locale/en-US.ini` + `id-ID.ini` | Progressive disclosure, settings migration, diagnostics; no refresh-per-event |

**Mandatory preparatory gate:** read the complete modern v18/v21/v23/v24 render ownership chain and the active PR #34 diff before editing one line. The table describes intended seams, not permission to modify each phase independently.

## 5. Click rendering design

### 5.1 Source behavior

Current ArZoom:
- four fixed `ClickVisualState` slots;
- per-video-tick `GetAsyncKeyState(VK_LBUTTON/VK_RBUTTON/VK_MBUTTON)` edge detection;
- bounded normalized content anchor;
- dual-ring shader.

Arssyut P4R.3:
- four fixed slots, but repeated same-button clicks within ~120 ms/near target **recharge** one existing slot;
- radius ~10.5→64 px at 1080-class output;
- one mathematically defined ring with a bright core, near bloom and diffuse halo derived from **the same distance field**;
- bright-surface support, explicit empty-center guard;
- per-kind reference lifetimes 0.88/0.90/0.84 seconds;
- no separate bloom render target.

### 5.2 Recommended adapter

1. Preserve ArZoom's click **event acquisition** initially; isolate its input accuracy/latency for a later measured task. The requested aesthetic work must not accidentally create a second input source.
2. Extend fixed-slot reducer with `try_retrigger(kind, mapped_content_point, elapsed)`. Select an existing active same-kind event in bounded O(4) time; reset age and generation atomically within the single tick owner. Otherwise recycle inactive/oldest existing slot.
3. Keep click lifetime in one typed source. Shader uses `progress = saturate(age / lifetime(kind))`. Remove the old lifetime mismatch; do not let CPU expire earlier than the shader fade.
4. Port Arssyut's `composite_single_ring` math to OBS `.effect` syntax and adapt the existing `float4 click_eventN` uniform ABI. Do not copy D3D11 constant-buffer layouts, sampler registers or full compositor.
5. Scale click dimensions by output short edge, with bounded factors (Arssyut reference clamp 0.85–1.60), and do proper bright/dark contrast testing.
6. Overlay uses the authoritative filter camera transform to project content once. If mapping invalid, do not guess coordinates or draw a ghost ring at frame center.
7. Preserve a `Classic Dual Ring` fallback style **only if** a compatibility/visual-test reason survives review; otherwise avoid keeping duplicate permanent rendering complexity. New premium should be the normal style.

**Caution:** ArZoom's current `phase2_capture_clicks` assumes its proven Single monitor path; never reuse this normalization in Multi. Multi must consume the canonical scene pointer defined by PR #34, only after its corresponding milestone is ready.

### 5.3 Cheap deterministic unit tests

- first left/right/middle click, all four slots occupied and oldest reuse;
- same-button same-location fast double click remains **one** active pulse;
- different button and genuinely different position produce distinct events;
- input aging at 30/60/120/144 Hz yields near-identical appearance and correct expiry;
- click anchor projected through camera zoom/pan and 4K output remains stable;
- no click when invalid mapping;
- no state/intent write to camera/Spotlight; benchmark allocation count stays zero.

## 6. Keycap semantics and privacy

Port/refactor only Arssyut's portable pieces (`presentation/shortcut.hpp`, `shortcut_visualizer.cpp` and associated deterministic tests), preserving:

- semantic enum and modifier ordering: Ctrl, Shift, Alt, Win, action;
- one action and up to four modifier keycaps, **max five**;
- unmodified text/number/space/punctuation never visualized or reconstructed;
- F1–F24, modifier chords, navigation/edit and supported media actions rendered;
- physical widths 1u/1.25u/1.5u/2u/3.5u;
- glyph length bounded (Arssyut uses 16 wchar cells per key label);
- 80 ms same-chord coalescing from Arssyut as a tuning starting point;
- time-based fade/bounce only as a transform/alpha of **one** retained atlas; no per-key resource regeneration.

Private content must not enter logging/diagnostics or a user settings file. Only derived current chord metadata and counters are allowed to exist in memory, capped and expired. No clipboard read, window-title sampling, text reconstruction or keystroke replay.

## 7. Input acquisition inside OBS — critical host-safety design

**Do not port Arssyut's `RegisterRawInputDevices` ownership verbatim.** Windows' official documentation specifies one registered target window per raw-input device class within a process and warns that registering from a library can interfere with the host application. OBS is the host process, not a standalone recorder.

**Recommended Windows candidate:** an optional module-level `WH_KEYBOARD_LL` observer with one owned message-loop thread, active only while at least one enabled/relevant keycap consumer needs it. This is a proposed backend subject to direct OBS compatibility testing, not a claim of universal hook reliability.

Lifecycle:
1. On demand, start one observer service and wait for an explicit Ready/Error result outside the video render callback.
2. The hook performs only bounded scan-key/transition normalization, posts compact transitions to fixed capacity/coalescing state, promptly calls `CallNextHookEx` and **never suppresses** a user's key.
3. Worker updates pressed-modifier state, deduplicates repeat/system chords, runs semantic privacy policy, publishes only a bounded latest chord/generation.
4. Consumer filter snapshots the newest semantic chord on its tick; it never waits for the hook or reads a mutable list guarded by a long lock.
5. Reference/consumer accounting ensures one observer for multiple filters, no duplicate hooks, and deterministic stop/join. OBS unload and Windows session change must not leave a callback pointing at destroyed code.
6. Failure (hook unavailable, denied, Windows secure desktop, missed system shortcut, queue saturated) yields explicit status + no keycaps. Camera/click/OBS UI stays healthy.
7. Do not use `RIDEV_NOLEGACY` or intercept normal shortcuts. Do not register Raw Input from the OBS plug-in until a host-safe coordination mechanism is proven.

Test input worker in an actual OBS process with active OBS hotkeys, gaming/administrative windows as applicable, Win+R/Win+D/Alt+Tab, duplicate keyboard events, OBS restart, plugin disable, scene switch and shutdown. If `WH_KEYBOARD_LL` proves unreliable in supported environments, document a capability-limited fallback; do not silently steal Raw Input registration.

Reference: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerrawinputdevices

## 8. Native keycap raster → OBS graphics resource

Do **not** copy Arssyut's D3D11 `ID3D11Texture2D` and SRV ownership into an OBS filter. OBS graphics owns the native API and device.

Recommended pipeline:

~~~text
ShortcutChord -> structured KeyboardOverlayFrame (<=5)
              -> generation-changed notification
              -> bounded CPU raster candidate (native GDI/DIB resources retained)
              -> graphics-thread upload to one GS_DYNAMIC gs_texture_t
              -> one output-space rect and opacity/scale snapshot
              -> existing PSZoom shader samples atlas only inside the rect
~~~

- The Arssyut physical-keycap proportions, thin border, light face, subtle depth, Windows four-pane glyph and content-bounded horizontal layout are the visual contract.
- Create/reuse one fixed-size CPU DIB, font, brushes and pens **outside the render hot path**; generate keycap pixels only on chord generation changes. Guard resource ownership and GDI cleanup.
- On graphics thread, create one `gs_texture_t` with `GS_DYNAMIC` once; use `gs_texture_set_image` on new prepared atlas generations only; no per-frame texture recreation. The API is documented at https://docs.obsproject.com/reference-libobs-graphics-graphics .
- Avoid blocking if CPU raster candidate is unavailable: keep last committed valid atlas until expired or display nothing.
- The keyboard overlay is **output-space**: bottom-center with safe margin and scale class; it must not grow or translate with camera zoom. Adjust for source dimensions and cropped per-source filter limitations.
- Reuse the existing filter shader's source sampling and visibility; one extra retained atlas sampler is acceptable **only when shader ABI and GPU measurement pass**. Alternative one simple overlay quad is a fallback design requiring explicit pass-count proof, not an accidental extra full-screen pass.
- Prefer pre-multiplied/straight alpha handling confirmed by pixel tests on both dark/white backgrounds. Do not assume Arssyut D3D11 color-space semantics exactly match OBS `GS_RGBA`.
- Retain small transparent texture/sampler fallback bound on **every** shared processed Draw, even while keycaps disabled, to prevent D3D11 unbound-shader-resource black frames.

## 9. Shader ABI, fail-safe and transactional state

Historical P5 bug: shared `Draw`/`PSZoom` parameters were not all initialized in non-Spotlight paths, yielding D3D11 `Not all shader parameters were set` and black output. Any keycap overlay must be designed against this exact failure mode.

Define a single `PreparedPresentationUniforms`/equivalent typed neutral packet:

- `zoom_amount` and `zoom_center` valid;
- all four `click_eventN` values initialized (inactive as zero);
- viewport dimensions valid;
- Spotlight uniforms initialized to neutral before feature override;
- Presentation Cursor atlas/size/position/visibility deterministic even if Off;
- Keycap texture **always valid** (transparent default), output rect valid nonzero, opacity zero when Off, scale valid;
- after validating all resources, one commit of coherent feature values; if candidate fails, retain last known good or pass through safely.

Every path into `obs_source_process_filter_end` must have the full shader ABI; no uninitialized union, null sampler or unvalidated GPU resource. Failures must not cause a property-sheet rebuild or silently rearm Spotlight.

**Shader prep validation:** enumerate `uniform` declarations, all `gs_effect_get_param_by_name` handles, every code path setting them, and one deterministic ABI test for feature matrix: zoom/click/cursor/spotlight/keycaps On/Off and resource failures. Check OBS D3D11 logs for missing params. Add a stable contract test similar to `tests/arzoom-p5-shader-abi-test.cpp`, not a one-off string grep only.

Reference: https://docs.obsproject.com/reference-sources and https://docs.obsproject.com/reference-libobs-graphics-effects

## 10. Zoom Follow parity and optional assist

### What each product actually runs

| Concern | ArZoom Scene Camera | Arssyut |
| --- | --- | --- |
| Semantic framing | SceneViewportPlanner on proven Display Capture→scene mapping | Per-source `PresenterAwareSmartCamera` via `ArZoomCameraAdapter` |
| Kinematics | SceneKinematicMotion, bounded jerk, tracking→settle continuity, exact HOLD | ArZoom gimbal/Cinematic policy for recorder frame |
| Activation | OBS Toggle/Hold/Zoom +/-/Overview/Reset/Freeze controls | Manual controls plus click-initiated temporary auto-zoom |
| Output | OBS scene-source filter, native scene composition | WGC/D3D11 recorder compositor |
| Current risk | Invalid/ambiguous OBS display mapping and shader ABI | Recorder-specific camera input and timing don't transfer directly |

**Decision:** ArZoom Scene Camera is stronger for the accepted OBS scene-wide use case and must remain default. Arssyut contributes a useful *intent policy*, not a new animation engine.

### Optional `ZoomIntentReducer` after A/B validation

Only if demanded by presenter testing, add a bounded policy feeding the existing camera:

~~~text
manual toggle / momentary hold / presenter control
   + optional click-intent time window
   + valid authoritative mapped pointer
   -> one effective zoom request
   -> existing SceneViewportPlanner / SceneKinematicMotion
~~~

Arssyut reference: zoom initiated by click, about 3 s hold and 0.75 s extension while pointer activity continues. Treat as configurable **experimental defaults**; do not make global defaults or hard-code them without direct OBS A/B.

Arbitration invariants:
- explicit Reset/Full Frame clears automatic intent; explicit Zoom Off cancels pending auto wake;
- Overview Peek preserves exact shot and returns correctly;
- Freeze holds exact visible camera shot; automatic click cannot unfreeze;
- invalid pointer mapping cannot trigger a guessed new camera target;
- camera zoom motion remains continuous and time-based;
- Spotlight consumes camera state read-only; click ring On/Off does not change zoom-trigger policy;
- no active new mode by default and no silent behavior change to existing scenes.

The optional mode can be rejected entirely if it increases accidental camera jumps. Evidence comes from equivalent scripted cursor traces **and** live presenter walkthrough, not an assumption that Arssyut's recorder policy is better.

## 11. Single/Multi and multi-filter ownership

PR #34's current design:
- existing `arzoom_filter` is **ArZoom Single** (stable v0.7.0 contract);
- `arzoom_filter_multi` is a separate new filter;
- Single XOR Multi camera authority per scene;
- the current draft head at review (`f17a11e60b321d3cf2f5b8688bf514339d33e48c`) reports M0/M1/M2A pure/structural work, pending M0 direct OBS acceptance;
- M3 canonical physical cursor→screen→UUID→source UV→scene point proof and M5 click/cursor/Spotlight wiring are future gates.

This enhancement should:
1. Change ArZoom Single overlay path only after baseline; **do not** edit PR #34 branch or revive the retired synthetic-monitor Multi experiment.
2. Design `PresentationOverlaySnapshot` to consume a canonical mapped pointer abstraction, while keeping the existing Single adapter. Do not create a competing Multi mapping implementation.
3. Reuse approved visual algorithms in Multi only after PR #34 M5, via its own canonical pointer and render ownership.
4. Keep keyboard input observer module-global and feature-private; it must not own or mutate camera topology.

## 12. Settings, migration and UI

Retain original saved `click_visual_enabled` and all established presenter/Spotlight/cursor keys. Add only narrow versioned settings:

- `click_visual_style` default `premium_single_ring` for the new release (with an optional `classic_dual_ring` migration choice in Advanced);
- `click_visual_size`, `click_visual_intensity` as small validated preset enums, not unrestricted shader controls;
- `shortcut_keycaps_enabled` default **false**;
- `shortcut_keycaps_size` default `standard`;
- `shortcut_keycaps_position` default `bottom_center`; initial version needn't expose arbitrary X/Y;
- `auto_focus_assist_enabled` default **false** only if later accepted and shipped.

UI runtime status has independent booleans/diagnostics: `renderer_ready`, `keycap_input_ready`, `mapped_pointer_valid`, `active_presentation_filter`. Do not claim active just because the checkbox is checked. Locale keys must exist in both EN and ID.

## 13. Test design

### Pure core (run fast on every PR)

- Click: four-slot cap; double click near/far cases; age and fade; 30/60/120/144 Hz parity; right/middle; no filled center via analytic/pixel checks.
- Shortcut: ordinary typing filtered; modifiers correctly ordered; Ctrl+Shift+Win+S, Alt+Tab, Win+R, F5/F24; no uncontrolled chord text; five-keycap cap; de-dup and generation.
- Geometry: 1u square and physical widths; bounds at 720p/1080p/4K; no all-screen panel.
- State: On/Off transitions, scene hide/reopen, missing hook, invalid mapping, missing atlas, startup and shutdown.
- Camera: all accepted P0–P5/P4.1 closeout/motion/mapping tests without weakened thresholds; click effects do not change camera output.
- ABI: every processed effect uniform and texture neutralized, per-feature combinations exercised; loss and recovery do not use arbitrary delay.

### Windows OBS integration

- Build/plugin load on supported OBS 31.1.1 and 32.1.2 when environment available, plus current supported OBS major via matrix decision.
- Test real capture at 1080p60; 4K60 only on available suitable hardware.
- Exercise click+cursor+Spotlight combinations on near-white and dark content, OBS scene switch, source add/remove, hidden/disabled filter, multiple sources and negative monitor coordinates; expected pass-through on unsupported geometry.
- Test keycap keyboard hook opt-in with OBS hotkeys and other foreground apps; no lost keys, duplicated events, or hook lifetime after uninstall/disable.
- Record before/after CPU render p50/p95, GPU frame cost, draw count, allocation churn, atlas uploads, frame drops, hook-to-visible latency, resource lifetime.
- Direct human sign-off on **recorded output**, not merely source screenshots/CI summary.

### Performance gate

No new full-screen draw pass, no per-frame heap allocation, GDI raster, font init, texture creation/destruction, CPU readback, filesystem work or synchronous diagnostics on rendering/video ticks; click=4 fixed states, keycaps≤5, one retained atlas, hook events bounded. Any numeric latency/regression budget must be set from paired measured machines and documented in the implementing PR.

## 14. Cutover and recovery

1. Baseline release and branch head fingerprint recorded; preserve a reference video of the existing stable scene.
2. Make click enhancement in a small isolated PR; retain a style rollback/feature flag during direct OBS evaluation.
3. Bring in portable keycap reducer and policy **before** OS input hook; tests first.
4. Integrate host-safe observer with explicit startup/shutdown and off-by-default privacy.
5. Prepare retained atlas and shared shader ABI **as one coherent reviewed change**. Neutral default allows no-keycap behavior.
6. Measure and run real OBS recordings; if black frame/hitch, revert the overlay seam without reverting the camera baseline.
7. Run optional camera-intent A/B separately; never block the core click/keycap release on experimental auto zoom.
8. Merge only after checks and direct OBS acceptance; preserve PR #34 independent state.

## 15. Exact phase exit gates

| Stage | Proof to attach to implementing PR | Abort conditions |
| --- | --- | --- |
| Click | deterministic click tests, before/after 1080p60 video, shader diff, CPU/GPU comparison | black frame, dual-ring remnants, center fill, double-click stack, camera behavior changed |
| Semantic keycaps | privacy/unit/Win-chord test outputs, bounded generation counters | ordinary typing visible or plaintext retained |
| Observer | Windows OBS opt-in hotkey and lifecycle recording, no lost keystrokes | host Raw Input collision, keyboard suppression, stale hook callback, OBS freeze |
| GPU keycaps | asset preview + OBS recording, shader ABI matrix, retained texture evidence | per-frame uploads/allocs, extra full-frame pass, unbound texture, keycap shifts with camera |
| Follow A/B | motion trace parity, live presenter validation, regression proof | pointer lost, target oscillation, unintended zoom, duplicate camera truth |
| Release | supported OBS versions, package/install, repeatable direct OBS sign-off | failed scene restoration, black output or incomplete validation |

## 16. Explicit limitations

- This is a **code-grounded architecture proposal**, not a compiled/tested implementation.
- The exact existing render ownership entry point must be revalidated against the live branch immediately before the first code PR; `v18/v21/v24` are candidate integration sites, not preapproved edit instructions.
- Arssyut's accepted clip proves its own recorder output, not OBS output or full GPU portability.
- The optional auto-zoom is intentionally not a shipped commitment until controlled comparison demonstrates value.
- ArZoom Multi remains a separate in-progress effort; its runtime implementation must not be inferred from this document.
