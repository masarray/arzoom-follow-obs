# Phase 4 — Zoominator audit and ArZoom scene-camera decision

> **Decision status: ACCEPTED AND SHIPPED.** This document records the architecture that became ArZoom Scene Camera in v0.5.0 and remains the baseline in v0.5.1. The earlier custom `ArZoom Camera` input-source / off-screen re-render experiment described below is historical context only and must not be revived as a current implementation plan. See [`PROJECT_DIRECTION.md`](PROJECT_DIRECTION.md) for current priorities.

## Why this audit exists

The first Phase 4 experiment registered a second OBS input type (`ArZoom Camera`) and manually re-rendered a selected scene/source into `gs_texrender_t`. Windows CI proved the code compiled, but direct OBS trial exposed unnecessary frontend/source-registration uncertainty before we had even reached the accepted ArZoom motion engine.

Before iterating on that architecture, Phase 4 audited the current public Zoominator implementation (`mmlTools/zoominator`) and the matching OBS frontend/source APIs.

## What Zoominator actually does

Zoominator does **not** register a scene-wide camera input source. `obs_module_load()` initializes a singleton frontend controller and adds `Tools → Zoominator`.

Its controller:

- gets the current OBS scene from the frontend;
- recursively enumerates scene items, including nested scenes;
- captures each item's original position / scale / rotation / bounds / crop;
- runs frame-critical transform work from `obs_add_tick_callback`;
- rewrites scene-item transforms during zoom/follow;
- persists a recovery map so original transforms can be restored after interruption/crash;
- samples UI/cursor information on the Qt/main thread and publishes a snapshot for the OBS tick path;
- installs native input hooks for global mouse/keyboard gestures.

This explains both Zoominator's scene-wide UX and a large part of its implementation complexity: scene mutation requires transform normalization, rotated/cropped item handling, write suppression, recovery persistence, scene-change recovery, and stale scene-item protection.

## Lessons ArZoom should keep

1. **Scene-wide must be presenter-first, not source-setup-first.**
   A Tools entry or similarly direct workflow is better than asking users to build a special source graph.

2. **Do frame-critical motion on OBS's video/tick path.**
   UI sampling and settings work stay on the frontend/main thread; camera state/render work stays on the OBS video path.

3. **Treat scene changes and lifecycle as first-class state transitions.**
   Never assume the same current scene or source survives a presentation.

4. **Map cursor space deliberately.**
   Do not assume an arbitrary source/window/display occupies the whole canvas.

5. **Fail safe when mapping is ambiguous.**
   Fixed framing / presenter controls are preferable to guessed mouse coordinates.

## What ArZoom intentionally rejects

### Persistent scene-item mutation

ArZoom will not implement scene-wide zoom by calling `obs_sceneitem_set_pos`, `obs_sceneitem_set_scale`, `obs_sceneitem_set_rot`, bounds setters, or crop setters on the user's composition.

That approach can work, but it creates exactly the recovery problem Zoominator has to solve. ArZoom's North Star is stronger: a crash or plugin failure must leave the user's scene transforms untouched because ArZoom never changed them.

### A second camera/motion engine

Phase 4 will not fork Smart Zone / Coast / SmoothIdle / Overview behavior. The accepted P1–P3.5 filter runtime remains the only presentation-camera engine.

## OBS capability that makes the better design possible

An OBS scene is itself an `obs_source_t` (`OBS_SOURCE_TYPE_SCENE`). OBS's filter UI treats scene sources as drawable video sources, and the official frontend code adds effect filters using:

```cpp
obs_source_filter_add(source, filter);
```

Therefore the accepted `arzoom_filter` can be attached directly to the **scene source**. OBS composes Display Capture, webcam, browser, logo, nested scenes, etc. first; ArZoom then processes the completed scene in the normal source-filter pipeline.

## Phase 4 architecture after the audit

```text
OBS Scene
├─ Display Capture
├─ Webcam
├─ Browser
├─ Logo
└─ Nested Scene
       ↓ OBS native scene composition
scene obs_source_t
       ↓ standard OBS effect-filter chain
ArZoom scene-level filter (same arzoom_filter runtime)
       ↓
Smart Zone / Presenter Controls / Overview / click / cursor
       ↓
final scene output
```

### Consequences

- no custom `ArZoom Camera` input type;
- no `gs_texrender_t` scene copy;
- no render recursion graph;
- no hidden scene item;
- no CPU frame readback;
- no persistent scene-item mutation;
- no second Smart Camera implementation;
- no source-list registration failure;
- scene rename is naturally safe because the filter belongs to the scene source;
- OBS serializes the filter with the scene collection using its normal filter path.

## P4 user workflow

Primary workflow becomes Tools-first:

- **Tools → ArZoom — Toggle Scene Camera**
  - creates an `arzoom_filter` named `ArZoom Camera` on the current scene if absent;
  - otherwise enables/disables that managed scene filter.

- **Tools → ArZoom — Configure Scene Camera**
  - ensures the managed filter exists;
  - opens OBS's normal Filters dialog for the current scene.

The existing per-source **ArZoom Filter** remains available and unchanged for the lightest Display Capture-only workflow.

## Input-space mapping contract

For the first scene-wide trial, Smart Follow/click/cursor mapping is allowed only when ArZoom can prove a deterministic Display Capture → scene mapping.

Initial supported case:

- exactly one visible Display Capture drives the presentation area;
- its scene-item box transform covers the scene canvas (normal fullscreen capture case);
- its monitor can be resolved deterministically.

If the Display Capture is arbitrarily cropped/scaled/rotated, or multiple display captures make mapping ambiguous, ArZoom must not guess. Presenter controls / fixed framing remain available while Smart Follow reports mapping unavailable.

The next mapping increment should use read-only scene geometry such as `obs_sceneitem_get_box_transform()` / `obs_sceneitem_get_draw_transform()` to support more complex layouts without writing transforms. This is now tracked as generalized read-only scene mapping, not as a replacement camera/render architecture.

## Why this is better than both previous approaches

Compared with Zoominator:

- same scene-wide presenter workflow;
- no scene transform mutation;
- no recovery map needed;
- keeps ArZoom's semantic Smart Zone and minimum-jerk motion;
- keeps Overview Peek, Freeze, premium click feedback and Presentation Cursor.

Compared with the first ArZoom P4-A source experiment:

- no second OBS input source registration;
- no off-screen scene re-render;
- no recursion guard/source graph required;
- no duplicated GPU composition pass;
- substantially smaller lifecycle surface;
- directly reuses the already accepted filter pipeline.

## Phase 4 engineering rule

> Prefer OBS-native composition/filter semantics over reconstructing an OBS scene graph inside ArZoom.

The scene is already a video source. Process it as one.

## Current follow-on direction

Phase 4 itself is complete. Future work must extend the accepted architecture rather than replacing it casually.

The immediate technical gap is **read-only coordinate mapping for more complex scene layouts**:

1. scaled/inset Display Capture mapping;
2. crop-aware mapping;
3. deterministic multi-capture target selection;
4. nested transform-chain support where it can be proven;
5. rotation support only with deterministic math and tests;
6. explicit diagnostic reasons whenever pointer mapping is unavailable.

Scene-item mutation, custom camera-source registration, and private scene re-rendering remain rejected unless a future architecture decision explicitly supersedes this document.
