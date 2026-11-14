# ArZoom Premium Presentation vNext — Product Requirements Document (PRD)

**Status:** PROPOSED / specification-only / requires approval and direct OBS visual acceptance before implementation is called complete  
**Date:** 2026-11-14  
**Target:** Windows OBS Studio plugin, ArZoom Single first; ArZoom Multi when its own canonical gates permit integration  
**Source repositories:** [ArZoom](https://github.com/masarray/arzoom-follow-obs) and [Arssyut](https://github.com/masarray/arssyut)  
**Companion:** [ARD](ARZOOM_PREMIUM_PRESENTATION_ARD.md)  
**Non-goal of this PR:** runtime source, shader, packaging, or release changes

## 1. Product intent

Turn ArZoom into a premium, easily operated live-presentation camera for OBS without compromising its accepted P4.1/P5 camera behavior or stability. Deliver three user-visible improvements:

1. **Click visual** that looks and feels at least as good as the accepted Arssyut P4R.3 emissive single ring, including consistent readable feedback over white/dark UIs.
2. **Realistic keyboard shortcut keycaps** based on Arssyut's accepted physical keycap appearance and privacy-respecting semantic chord reducer.
3. **Smarter Zoom Follow experience** based on a careful comparison of ArZoom and Arssyut semantics; no blind replacement of ArZoom's established scene camera or mapping engine.

Success means better video output, calmer and more predictable presenter interaction, lower setup friction, and no new OBS black-frame, input, or rendering regression.

## 2. Evidence and source-of-truth

Verified against repository source on 2026-11-14:

| Item | Actual code/evidence | Implication |
| --- | --- | --- |
| ArZoom scene camera | `src/arzoom-scene-viewport-planner.hpp`, `src/arzoom-scene-motion-synthesizer.hpp`, `docs/P4_1_STABLE_BASELINE.md` | Preserve semantic WHERE and kinematic HOW; no second camera authority |
| ArZoom click | `src/arzoom-click-visual.hpp`, `src/arzoom-filter-v3.cpp`, `data/effects/arzoom.effect` | Four fixed click slots, per-tick Windows button sampling, dual-ring shader |
| ArZoom render chain | `CMakeLists.txt` compiles `src/arzoom-filter-v24.cpp`, which includes previous filter phases | Avoid another ad hoc wrapper or unreviewed shader ABI changes |
| ArZoom P5 | `src/arzoom-filter-v21.cpp` through `v24.cpp`, `docs/P5_3_SAFE_ACTIVATION_HANDSHAKE.md` and `docs/P5_4_D3D11_SHADER_ABI_RECOVERY.md` | Every shared Draw-path shader uniform and texture must be valid even when an effect is Off |
| Arssyut camera | `src/presentation/arzoom_camera_adapter.hpp` | Reuses ArZoom's `PresenterAwareSmartCamera`, configured Cinematic; not an independent superior scene planner |
| Arssyut auto-zoom intent | `src/presentation/presentation_controller.cpp` | Click-triggered temporary zoom uses a 3 s hold and 0.75 s motion tail; consider only as optional behavior |
| Arssyut premium click | `src/platform/windows/graphics/d3d11_compositor.cpp` and `docs/P4R3_PRESENTATION_RELIABILITY.md` | One luminous ring, center guarded against fill, bright-surface contrast, same-target retrigger coalescing |
| Arssyut keycaps | `src/presentation/shortcut.hpp`, `shortcut_visualizer.cpp`, compositor, `docs/P4R_KEYBOARD_VISUALIZER.md`, `docs/P4R3_PRESENTATION_RELIABILITY.md` | Five bounded physical keycaps, native retained rendering, generation-based updates, ordinary typing hidden |
| ArZoom Multi in flight | [Draft PR #34](https://github.com/masarray/arzoom-follow-obs/pull/34), canonical Multi handoff | Never cross-wire the stable Single mapping with the new Multi topology/UUID authority |

Arssyut's vendored `third_party/arzoom/include/arzoom-click-visual.hpp` has the **same blob SHA** as ArZoom's `src/arzoom-click-visual.hpp` (at audit time). Its visual difference is primarily in its actual compositor shader and controller retrigger policy, not in that vendored header.

Arssyut's accepted P4R.3 direct 1080p60 recording is historical project evidence, **not** proof that the proposed OBS port has already been visually accepted.

## 3. Users and workflows

**Primary:** tutorial creators, live software demonstrators, lecturers, automation engineers presenting HMI/SCADA, and OBS screen-recording users who need viewers to see small UI details.

**Workflow A — beginner:** add ArZoom Scene Camera, select Quick Setup, enable Click Feedback, optionally enable Shortcut Keycaps, use one existing Zoom hotkey. No technical mapping/graphics knowledge required.

**Workflow B — advanced live presentation:** use Toggle/Hold/Zoom +/-/Overview/Freeze without unintended camera movement from click animation or keycaps. Optional future Auto Focus Assist may zoom from an explicit click; the default remains predictable.

**Workflow C — teaching shortcuts:** show Ctrl+C, Ctrl+Shift+S, Win+R, F5 in legible small physical keys at the bottom of output. Ordinary typed letters/password content must not create a transcript or text overlay.

**Workflow D — multiple filters:** existing ArZoom Single scenes continue to load. ArZoom Multi remains governed by PR #34 and receives these visual features only after its own canonical pointer/M5 gates are ready.

## 4. Priorities and functional requirements

### P0 — Premium Click Feedback (must ship first)

| ID | Requirement | Acceptance |
| --- | --- | --- |
| CLICK-01 | Default premium look: one expanding emissive ring, with no dot, no filled center, and no visible second ring | User approves direct OBS recording on dark UI and white browser/document |
| CLICK-02 | Left electric sky `#32B8FF`; right rose `#FF5C8A`; middle warm amber `#FFC857` | Button identity is clear without labels |
| CLICK-03 | Initial 1080-class geometry follows Arssyut P4R.3: radius roughly 10.5 to 64 px, core half-width 3.9 to 2.25 px, near/diffuse glow | Remains legible at 720p, 1080p, 1440p, and 4K without cartoonish thickness |
| CLICK-04 | Use Arssyut baseline fade envelopes (roughly 0.84–0.90 s) and smooth expansion as starting values, then tune only from OBS real recording | No hard pop, abrupt cut, or stale ring |
| CLICK-05 | Same kind + close target + within short retrigger window (~120 ms baseline) refreshes one ring, never stacks concentric duplicate geometry | Double-click remains clean; distinct targets still show separately |
| CLICK-06 | Keep bounded four-event capacity and content-space anchoring projected by the current authoritative camera | Click stays on the clicked content while camera pans/zooms |
| CLICK-07 | Click visual cannot change camera target, velocity, acceleration, Zoom/Spotlight intent, or scene-item transforms | Existing camera tests and traces remain unchanged |
| CLICK-08 | Camera/filter idle pass-through retained; graphics resources must fail safely | No black preview, failure to toggle Spotlight to recover, or OBS crash |
| CLICK-09 | Simple user controls: Click Feedback On/Off, Style Premium / Classic (classic in Advanced), optional Size/Intensity presets | New premium look is discoverable; no 10-slider setup panel |

The precise pixel thresholds and blend curves are **reference values from Arssyut**, not hard-coded final requirements until real OBS output is reviewed.

### P1 — Shortcut Keycap Visualizer

| ID | Requirement | Acceptance |
| --- | --- | --- |
| KEY-01 | Optional **Shortcut Keycaps** feature, Off by default for privacy | Existing scenes, users, streams never suddenly expose keyboard activity |
| KEY-02 | Port structured shortcut semantic policy, modifier order Ctrl → Shift → Alt → Win → action | Exact semantic labels, not raw pressed-key dump |
| KEY-03 | Suppress plain unmodified letters, numbers, Space, punctuation; show modified actions, F1–F24, navigation/edit/media actions as designed | No typing transcript, clipboard, foreground-field text, or persisted key history |
| KEY-04 | Maximum five concurrent keycaps, same white physical-key language | No oversized card/background panel |
| KEY-05 | Physical widths: 1u=82, 1.25u=103, 1.5u=123, 2u=164, 3.5u=287 px at source raster baseline; 82 px face height | Ctrl/Alt, Shift/Enter, Backspace, Space and Windows logo look physically coherent |
| KEY-06 | Bottom-center, content-bounded overlay with a safe output-space margin, proportional scale, independent of camera zoom | Readable at 720p–4K, does not move with camera pan |
| KEY-07 | Retained raster/texture updated only when a displayed chord generation changes; fade/bounce only changes transform/alpha | No per-frame GDI/font creation or texture upload |
| KEY-08 | Time-based fade-in (~70 ms), hold (~1.2 s), fade-out (~220 ms), short ~320 ms cluster emphasis are reference behavior | No flicker on key autorepeat or repeated chord |
| KEY-09 | If host input acquisition is denied, conflicting, unsupported, or shut down, disable keycaps only with a clear status | No audio/video interruption, no keyboard interception, no silent host-input takeover |
| KEY-10 | One module-level input observer at most, never one OS hook per filter | Multiple scene/filter instances cannot multiply global hooks |

**Important usability boundary:** scene-level filter shows keycaps relative to the OBS scene output. Per-source filter may produce keycaps only inside that source's rendered area; do not promise a global bottom-of-scene overlay from a per-source filter.

### P2 — Zoom Follow behavior refinement

1. **Keep the stable default**: ArZoom P4.1 SceneViewportPlanner (WHERE) and SceneKinematicMotion (HOW), Smart Zone semantics, direct OBS accepted tracking/settling, exact HOLD, Overview Peek, manual hotkeys, scene mapping and P5 cinematic interaction.
2. Reuse **Arssyut's intent lesson**, not its camera as a drop-in replacement: optional **Auto Focus Assist** may request a temporary zoom after a meaningful click. Start from its approximately 3 s click zoom / 0.75 s motion-tail policy only as a controlled A/B prototype.
3. Avoid surprising live zoom: Auto Focus Assist is **Off by default**, disarmed on Reset/explicit Zoom Off or missing valid mapping, and cannot supersede Freeze/Overview/manual presenter intent.
4. Click feedback On/Off must remain independent from auto camera intent: a ring can appear without zoom and zoom can run without rings.
5. Any enhancement must pass existing 2×/4× pointer acquisition, reversal, jitter, hysteresis, braking, stop/restart, and exact HOLD tests before a default behavior change is considered.

A new auto-zoom control is a **later decision gate**. This PRD does not mandate enabling it in the next release.

## 5. UX / properties hierarchy

Use existing OBS properties; avoid a second configuration UI or modal windows.

- **Quick Setup:** existing Enable ArZoom, Zoom Amount, Camera Focus, Motion / Preview / Reset.
- **Presentation Feedback:** Click Feedback [On]; click style [Premium Single Ring] (Classic in Advanced); Shortcut Keycaps [Off]; concise privacy note.
- **Spotlight / Presenter Controls:** preserve P5 defaults and hotkey semantics.
- **Advanced (collapsed):** click size/intensity presets, keycap size (Compact/Standard/Large), safe bottom margin, optional future Auto Focus Assist (experimental), input diagnostics.

No OBS property-sheet rebuilding per animation frame or hotkey event. Runtime status should reflect actual input-hook/renderer readiness, not merely an enabled checkbox.

## 6. Compatibility and safety

- Retain `arzoom_filter` internal OBS filter ID; existing saved sources, hotkeys, profile settings and default behavior must load.
- No new camera source type, no rewritten scene-item transforms, no CPU video readback, no duplicate scene render.
- Single and Multi remain separate camera authorities (Single XOR Multi on one scene); do not touch PR #34's canonical Multi architecture or bring back synthetic-monitor mappings.
- Keycap capture and logging must be opt-in, bounded, non-text-reconstructing; unsubscribe deterministically on unload/shutdown.
- Windows low-level hook must never block, suppress, or inject input. If input is unavailable, visuals are disabled without affecting Zoom or Click.
- Existing cursor preset/hotspot behavior and Spotlight shader fallback must remain unaffected.
- No new runtime dependency until binary size, license, compatibility, and performance are evaluated.
- UI EN/ID locale strings; no hard-coded English-only new properties.

## 7. Quality gates / Definition of Done

**Automated:** pure click retrigger/lifetime/projection tests; shortcut semantic/privacy/keycap-layout tests; deterministic event-generation/repeat/drop gates; shader ABI all-modes validation; graphics lifecycle tests; all existing P0–P5/P4.1 tests untouched; cold-start and shutdown/reconfigure negative tests.

**Performance:** baseline and candidate measured at identical resolutions, OBS output FPS, scene content, and GPU; capture median and p95 CPU/video-render cost, GPU pass count, allocations, resource creation/upload count, shader compile count, hook-to-visible latency and 30-minute resource trend. Hard invariants: zero new full-scene passes, zero per-frame texture/font creation or dynamic allocation in render paths, bounded queues/states, no sustained frame drops attributable to overlays. Proposed numeric budgets must be established by actual reference machine measurements, not invented.

**Direct Windows OBS:** 1920×1080/60 and 3840×2160/60 if available, OBS 31.x and 32.x/next supported major where obtainable; dark editor/IDE, near-white browser/Excel, dense UI, click + Zoom + Cursor + Spotlight On/Off; repeated double click; Ctrl+C/Ctrl+Shift+S/Alt+Tab/Win+R/F5 plus ordinary typing; source hide/show, restart, scene switch, filter add/remove, simultaneous source filter and managed Scene Camera warning, invalid mapping, shader/resource failure and scene restore.

**ArZoom Multi:** M0 direct OBS pass-through is presently pending in Draft PR #34; do not report Multi overlay compatibility as passed until its M3/M5 pointer and presentation gates are complete.

**Release criterion:** a real output recording is reviewed and accepted for aesthetics and stability. CI PASS and shader compile alone never constitute visual acceptance.

## 8. Implementation order and ownership

| Milestone | Scope | Exit gate |
| --- | --- | --- |
| M0 — baseline lock | Inventory exact main and PR #34, record direct OBS reference, attach screenshots/clip and metrics | No false claims of baseline; One known camera/shader authority |
| M1 — Click V2 | Port Arssyut P4R.3 single-ring math, clean same-target retrigger, minimal OBS shader adapter | CI + dark/white click video approved |
| M2 — Shortcut semantic/input | Port portable policy and physical keycap model; implement OBS-safe optional input observer | Privacy, Win chords, failure, lifecycle tests |
| M3 — Keycap render/UX | Retained keycap atlas and OBS shader texture, locale/settings, compact visual polish | Direct OBS visuals + shader ABI + performance |
| M4 — camera A/B | Benchmark current ArZoom against optional Arssyut-style click-intent reducer; no default swap | Deterministic traces + presenter trial; may be cancelled |
| M5 — integration/release | Single stability, release documentation/package, Multi readiness assessment | Explicit direct OBS sign-off |

Each milestone should be a coherent reviewable PR with one owner, tests, measured evidence and a rollback path. Do not bundle an unproven camera redesign into the click/keycap shipment.

## 9. Risks and open decisions

1. **OBS host Raw Input ownership:** Windows documents one registered target per device class in a process and explicitly warns libraries not to register Raw Input independently. The ARD selects a separate opt-in keyboard capture strategy; implementation still needs a host-conflict trial.
2. **Shader ABI:** adding a texture without fully neutralizing every Draw path can recreate the historical D3D11 black-preview bug.
3. **Different rendering APIs:** Arssyut's D3D11 shader and GDI texture cannot be pasted verbatim into OBS `gs_effect`; port semantics and visual math, adapt resources.
4. **Live camera surprise:** a click-triggered auto zoom helps recordings but could be disruptive in a live stream. Must remain opt-in until measured and human-reviewed.
5. **Multi concurrency:** PR #34 is an active isolated camera/topology effort; its next gate, not this project, determines when Multi overlay integration is safe.
6. **Native cursor:** Arssyut's accepted P4R.3 uses a captured Windows cursor; ArZoom has seven native presentation-cursor presets. Do not replace or duplicate ArZoom's cursor while porting click/keycaps.

## 10. Scope status

This document is an audited **implementation proposal**. No plugin code, shader, Windows installer or runtime has been modified or validated by this documentation milestone. Do not mark implementation complete until each acceptance gate is executed.
