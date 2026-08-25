# P5 — Smart Focus Spotlight + Beginner-First GUI

**Status:** planned product/engineering contract after ArZoom v0.6.0. Not shipped yet.

**Purpose:** define the intended premium Spotlight feature and the GUI simplification required to expose it without making ArZoom harder for first-time OBS users.

Read first:

1. [`PROJECT_DIRECTION.md`](PROJECT_DIRECTION.md)
2. [`P4_1_STABLE_BASELINE.md`](P4_1_STABLE_BASELINE.md)
3. [`SMART_CAMERA_ARCHITECTURE.md`](SMART_CAMERA_ARCHITECTURE.md)
4. [`ARCHITECTURE.md`](ARCHITECTURE.md)

P5 must extend the accepted v0.6.0 Scene Camera architecture. It must not weaken P4.1 mapping/motion behavior, create a second Smart Camera, mutate OBS scene items, or introduce a second scene render.

---

## 1. Product goal

ArZoom Spotlight should make live tutorials, engineering demonstrations, software walkthroughs, online classes, and recorded OBS presentations look edited by a professional video editor while remaining lightweight enough to leave enabled during normal presentation work.

The intended visual language is:

- the important area remains clear and natural;
- the surrounding area becomes gently darker, not blacked out;
- edges are soft and premium rather than hard or game-like;
- motion is calm and intentional;
- local cursor gestures do not create distracting spotlight jitter in Smart mode;
- Spotlight remains visually synchronized with the accepted ArZoom camera rather than behaving like an unrelated overlay.

The feature must feel like **attention guidance**, not a special effect.

### North-star example

```text
Presenter explains local area A
        ↓
Camera remains calm
Spotlight remains calm
        ↓
Presenter moves to meaningful area B
        ↓
SceneViewportPlanner identifies new context
        ↓
Camera moves using accepted kinematics
Spotlight visually follows the same presentation intent
        ↓
Camera reaches exact HOLD
Spotlight also settles and becomes still
```

---

## 2. Non-goals

P5 Spotlight is **not**:

- AI object detection;
- OCR or image understanding;
- a face tracker;
- a second semantic camera/planner;
- a blur-behind-focus effect;
- a particle/glow/bloom system;
- a PNG/browser-source overlay;
- a new OBS helper source;
- a reason to rewrite scene-item transforms;
- a reason to duplicate scene rendering;
- a reason to weaken pass-through when Spotlight is disabled.

Do not pursue visual complexity that requires frame readback, image analysis, multi-pass blur, temporary scene copies, unbounded event history, or per-frame allocations.

---

## 3. Exactly three user-facing Spotlight modes

The initial P5 product exposes exactly three Spotlight behavior modes. Shape, feather, strength, and size are appearance parameters, not additional behavior modes.

### 3.1 Smart Focus — recommended default

**User promise:** “Highlight what ArZoom believes I am currently explaining.”

Smart Focus consumes presentation intent already produced by the accepted camera system. It must not invent a competing interpretation of pointer behavior.

Behavior contract:

- local pointer motion inside useful context may produce **zero Spotlight relocation**;
- meaningful context relocation may move the Spotlight;
- the Spotlight follows the same semantic focus used by `SceneViewportPlanner` where available;
- during camera tracking/settling, Spotlight follows the semantic focus through the current camera transform;
- when camera intent reaches exact HOLD, Spotlight must also become visually still;
- Smart Focus never wakes, retargets, accelerates, or otherwise influences the camera;
- Smart Focus does not create its own cursor-mapping heuristic.

The dependency direction is one-way:

```text
Read-only mapped input
        ↓
SceneViewportPlanner
        ↓
Camera target / semantic focus
        ├────────────→ SceneKinematicMotion
        └────────────→ Smart Spotlight target
```

Never reverse this dependency.

### 3.2 Cursor Spotlight

**User promise:** “Keep the bright focus area around my pointer.”

Behavior contract:

- center follows the proven mapped cursor position;
- a small time-based visual smoothing stage removes raw pointer jitter;
- smoothing is visual-only and O(1);
- cursor motion does not gain authority over camera policy merely because Cursor Spotlight is enabled;
- if pointer mapping is unavailable or ambiguous, do not guess;
- hold the last valid position or use an explicitly documented safe fallback and surface mapping status in the GUI/log.

Cursor Spotlight is intentionally more reactive than Smart Focus and is suitable for fast UI demonstrations where the presenter wants direct manual emphasis.

### 3.3 Click Spotlight

**User promise:** “Click an area to lock the audience’s attention there.”

Behavior contract:

- a valid presentation click captures one content-space focus anchor;
- the focus remains locked to that content location until the next valid focus click, reset, or Spotlight disable;
- when the ArZoom camera pans/zooms, the locked focus stays attached to the same presentation content;
- moving to a newly clicked focus uses a short premium visual transition instead of an instantaneous jump;
- click focus must not wake or retarget camera motion solely because Spotlight is enabled;
- existing click-feedback rendering and Click Spotlight may share the same click event but remain independent consumers.

Click Spotlight must use bounded constant-size state: one active anchor plus transition state. No click history is required.

---

## 4. Spotlight visual contract

The default look should be subtle enough for long tutorial sessions and strong enough to guide attention immediately.

### 4.1 Rendering model

The preferred visual is an analytic soft mask evaluated in the existing ArZoom presentation shader/pass.

Conceptually:

```text
sourceColor = sample(sceneTexture)
mask        = analytic_focus_mask(outputPixel, center, size, feather, shape)
outside     = smoothstep(innerBoundary, outerBoundary, maskDistance)
dimFactor   = 1.0 - outside * dimStrength
finalColor  = sourceColor.rgb * dimFactor
```

The center remains at original brightness. The outside region is multiplicatively dimmed.

### 4.2 Default appearance

Initial recommended defaults:

- Spotlight master: **Off** for existing-user compatibility;
- first enabled mode: **Smart Focus**;
- background dim strength: approximately **38%**;
- feather: broad/soft, approximately **10–14% of the shorter output dimension**;
- default size: **Balanced**;
- transition fade-in: approximately **140 ms**;
- transition fade-out: approximately **120 ms**;
- no bloom;
- no blur;
- no hard outline;
- no pure-black outside region;
- no color tint by default.

Exact constants may be tuned through direct OBS trials, but the product character above is part of the contract.

### 4.3 Size presets

Beginner GUI should prefer three understandable presets instead of exposing raw width/height first:

- **Compact** — precise controls, buttons, small code regions;
- **Balanced** — recommended default for most tutorial work;
- **Wide** — forms, timelines, code blocks, engineering diagrams.

Advanced controls may expose exact normalized dimensions later without changing stored preset compatibility.

### 4.4 Shape

Shape is an advanced appearance choice, not a fourth behavior mode.

Initial supported shapes should remain cheap and analytic:

- **Soft Ellipse**;
- **Soft Rounded Rectangle**.

The default should be selected through direct OBS trials. Rounded Rectangle is likely preferable for software/tutorial content; Ellipse remains useful for pointer-centric explanation.

Do not add image masks or arbitrary texture masks in P5.

### 4.5 Coordinate behavior

Spotlight center and Spotlight size intentionally use different semantics:

- **center** is presentation/content anchored when the mode requires it;
- **size** is output-space stable so the perceived Spotlight size does not inflate unpredictably as camera zoom changes.

For Click Spotlight, a locked content coordinate is transformed by the current camera every frame so the light remains on the same content.

For Cursor Spotlight, the mapped pointer is transformed into final presentation output coordinates.

For Smart Spotlight, the semantic focus point is transformed through the current accepted camera state.

All three modes must share the same proven coordinate pipeline where relevant. Do not create a separate coordinate resolver for Spotlight.

---

## 5. Spotlight state architecture

P5 may add a compact presentation-effect state, for example conceptually:

```text
SpotlightState
├─ enabled
├─ mode
├─ target_center
├─ visual_center
├─ locked_content_anchor       // Click mode only
├─ opacity / fade state
├─ size preset / normalized size
├─ dim strength
├─ feather
└─ shape
```

Requirements:

- O(1) bounded state;
- no growing pointer/click history;
- no heap allocation in normal per-frame update;
- time-based behavior, not frame-count-based behavior;
- visual smoothing may use a bounded exponential/critically-damped state;
- visual smoothing is not a semantic planner;
- reset/disable produces deterministic state.

### Camera isolation invariant

Spotlight state may read:

- current camera transform;
- current semantic focus target;
- mapped pointer snapshot;
- bounded click event state;
- presenter controls/settings.

Spotlight state must never write back into:

- `SceneViewportPlanner` intent state;
- follow pressure;
- tracking latch;
- camera target selection;
- camera velocity/acceleration;
- OBS scene-item transforms.

---

## 6. GPU/performance contract

P5 is accepted only if the premium look is achieved without materially changing ArZoom’s lightweight architecture.

### Required hot-path properties

- Spotlight disabled: preserve existing pass-through behavior where possible;
- Spotlight enabled: remain inside the existing ArZoom presentation render path;
- no second scene render;
- no second OBS source graph;
- no CPU frame readback;
- no image/OCR/AI processing;
- no blur texture pass;
- no particles;
- no per-frame file I/O;
- no per-frame settings writes;
- no unbounded event containers;
- no avoidable heap allocation in tick/render;
- prefer one source texture sample and analytic mask math in the same shader pass;
- click/cursor/spotlight uniforms should be compact and bounded.

### Performance acceptance

Do not publish universal millisecond or percentage claims from one machine.

Instead, P5 acceptance requires:

1. architecture review confirms no extra scene render/readback/blur pass;
2. existing CPU microbenchmarks remain within normal regression tolerance;
3. add a Spotlight state-update benchmark;
4. add a shader/render smoke test where practical;
5. direct OBS trials at 30/60/120/144 fps;
6. direct trials on Intel/AMD/NVIDIA where available;
7. verify no new idle CPU activity when Spotlight is disabled;
8. verify Spotlight motion remains frame-rate independent.

---

## 7. Beginner-first GUI strategy

The current properties page exposes status, Enabled, Zoom Amount, Mouse Follow, Movement, Safe Zone, hotkey status/buttons, Test/Reset, then an Advanced group. P5 should reorganize the page so the common workflow requires very few decisions.

### UX principle: progressive disclosure

A first-time user should understand the top half of the dialog without knowing what “safe zone”, “anchor X/Y”, capture mapping, or transform geometry means.

Target common path:

```text
Install
  ↓
OBS Tools → Enable ArZoom Scene Camera
  ↓
Configure
  ↓
Zoom Amount
Camera Focus = Smart Follow (Recommended)
Spotlight = On → Smart Focus (Recommended)
  ↓
Present
```

### 7.1 Proposed top-level property structure

```text
STATUS
  Ready / warning / mapping explanation

QUICK SETUP
  Enable ArZoom
  Zoom Amount
  Camera Focus
  Camera Motion
  Preview Zoom
  Reset View

SPOTLIGHT
  Enable Spotlight
  Spotlight Mode
  Focus Size
  Background Dim
  Preview Spotlight

CONTROLS
  Hotkey status
  Open OBS Hotkeys

ADVANCED  [collapsed by default]
  Safe Zone
  Target Monitor
  Anchor X / Y
  Reset When Hidden
  Spotlight Shape
  Spotlight Feather
  advanced visibility/transition values if later needed
  diagnostics / mapping reason
```

### 7.2 Beginner labels

Prefer user-language labels while keeping existing stored values for backward compatibility.

Recommended label direction:

| Existing concept | Beginner-facing label |
|---|---|
| Mouse Follow | **Camera Focus** |
| Smart | **Smart Follow (Recommended)** |
| Centered | **Center on Pointer** |
| Fixed | **Fixed Frame** |
| Movement | **Camera Motion** |
| Smooth | **Smooth (Recommended)** |
| Balanced | **Balanced** |
| Fast | **Responsive** |
| Safe Zone | **Advanced: Calm Area / Safe Zone** |
| Anchor X/Y | **Advanced: Focus Anchor X/Y** |

Changing a display label must not silently change the stored setting schema or accepted camera behavior.

### 7.3 Spotlight controls for beginners

Basic Spotlight group should initially expose only:

1. **Enable Spotlight** — master toggle;
2. **Spotlight Mode** — Smart Focus / Cursor / Click;
3. **Focus Size** — Compact / Balanced / Wide;
4. **Background Dim** — simple percentage slider with a safe range;
5. **Preview Spotlight** — deterministic preview/test action.

Everything else belongs under Advanced.

Recommended mode copy:

- **Smart Focus (Recommended)** — “Highlights the area ArZoom believes you are explaining.”
- **Cursor** — “Follows your pointer directly.”
- **Click** — “Click an area to keep the focus there.”

Tooltips should explain behavior in one sentence and avoid implementation vocabulary.

### 7.4 Dynamic property visibility

Use OBS property-modified callbacks where practical so irrelevant controls disappear.

Examples:

- Spotlight disabled → hide mode/size/dim/shape/feather details, keep only enable + short explanation;
- Spotlight enabled → reveal basic Spotlight controls;
- Click mode → show click-specific help/status only if needed;
- Smart mode → do not expose cursor smoothing knobs;
- Advanced group remains collapsed by default.

Do not add a separate “Simple Mode / Expert Mode” state unless progressive groups prove insufficient. A separate mode creates another setting and support burden.

### 7.5 Hotkey simplification

The GUI should not make a beginner think hotkeys are mandatory for basic operation.

Direction:

- keep **Open OBS Hotkeys** as the primary control;
- continue automatic profile persistence where possible;
- move manual “Save Hotkey Now” into Advanced or remove it once no longer technically necessary;
- add a future **Toggle Spotlight** presenter hotkey if implementation proves useful;
- do not add many Spotlight-specific hotkeys in P5 v1.

### 7.6 Status should explain, not diagnose cryptically

The top status field should prefer actionable messages:

- “Ready — Smart Follow and Spotlight available.”
- “Ready — camera works; pointer mapping unavailable for this scene.”
- “Multiple Display Captures detected — choose a presentation target.”
- “Spotlight Cursor mode is waiting for valid pointer mapping.”

Detailed geometry/debug information remains in logs or Advanced diagnostics.

---

## 8. Defaults and backward compatibility

P5 must not alter the visual output of existing ArZoom installations merely because they update.

Therefore:

- Spotlight defaults **Off**;
- existing zoom/follow/movement defaults remain unchanged unless separately approved;
- existing setting keys and values remain readable;
- GUI label improvements may map onto existing stored values;
- adding Spotlight settings must have explicit defaults;
- old scene collections without Spotlight settings load safely;
- disabling Spotlight restores the same output path/behavior as v0.6.0 except for unrelated approved fixes.

Suggested settings namespace:

```text
spotlight_enabled
spotlight_mode              // smart | cursor | click
spotlight_size              // compact | balanced | wide
spotlight_dim_percent
spotlight_shape             // ellipse | rounded_rect
spotlight_feather_percent
```

Names are proposals, not ABI until implementation lands. Once released, preserve compatibility.

---

## 9. Mapping and fail-safe behavior

P5 inherits the mapping scope of the ArZoom version it is built on.

It must not delay implementation waiting for universal mapping, but it must also not create Spotlight-specific guesses.

### On the v0.6.0/P4.1 mapping scope

- Smart Spotlight may consume the same proven scene presentation target and planner focus;
- Cursor Spotlight requires a valid mapped pointer;
- Click Spotlight requires a valid mapped presentation click/content anchor;
- unsupported mapping cases fail safe and explain why.

### With future P4.2 multi-capture selection

Spotlight should automatically benefit from the selected/proven presentation target because it consumes the shared mapping layer.

This is why Spotlight must not implement its own source-owner resolver.

---

## 10. Interaction with existing presentation effects

### Presentation Cursor

Presentation Cursor and Spotlight may coexist.

- cursor remains crisp and visible inside/outside the soft focus boundary;
- Spotlight must not double-transform the cursor;
- both consume the same mapped presentation coordinates where relevant.

### Click feedback

Click rings remain a short event visualization. Click Spotlight is a persistent attention state.

One click may feed both consumers:

```text
click event
   ├─→ bounded click-ring animation
   └─→ Click Spotlight locked anchor
```

The ring may fade while the Spotlight remains.

### Overview Peek / Reset / Freeze

- Overview Peek changes camera framing but must not corrupt a Click Spotlight content anchor;
- Reset View must produce deterministic Spotlight behavior documented by mode;
- Freeze Camera freezes camera movement only; it does not automatically change Spotlight mode;
- disabling the ArZoom filter clears transient Spotlight visual state safely.

Exact UX for Overview/Reset should be validated in direct OBS trials before release.

---

## 11. Required deterministic tests

Add tests that treat Spotlight behavior as a product contract.

### Core state tests

- disabled Spotlight is visually/state inert;
- enable/disable fade is deterministic and time-based;
- size/strength values remain bounded;
- state contains no growing history;
- reset clears transient state deterministically.

### Smart Focus tests

- local pointer work does not relocate Smart Spotlight when semantic camera focus remains unchanged;
- meaningful planner target change produces one coherent Spotlight target change;
- camera HOLD produces Spotlight HOLD;
- Spotlight cannot alter planner output, follow pressure, or camera kinematics.

### Cursor tests

- cursor mapping produces correct output-space center at 1×/2×/4×;
- camera pan/zoom transform is applied exactly once;
- invalid mapping does not guess;
- smoothing remains equivalent across frame rates.

### Click tests

- one click captures one content anchor;
- anchor stays on the same content while camera moves;
- next click relocates focus once;
- reset/disable clears or preserves state exactly as specified;
- click-ring animation cannot move the locked focus after capture.

### Rendering/math tests

- center remains full brightness within numeric tolerance;
- outer dim never exceeds configured bound;
- feather is monotonic and has no hard discontinuity;
- ellipse and rounded-rectangle SDFs remain inside output bounds;
- no invalid values at tiny/large output sizes.

---

## 12. Direct OBS acceptance scenarios

P5 is not accepted from unit tests alone.

Test at minimum:

1. 1080p software tutorial with frequent small pointer gestures;
2. 1440p/4K engineering diagram or SCADA/software demonstration;
3. code editor with fast pointer relocation;
4. Click Spotlight while camera zooms and pans;
5. Smart Spotlight at 2× and 4×;
6. Overview Peek with Spotlight active;
7. scene change while Spotlight is active;
8. enable/disable stress;
9. mixed DPI multi-monitor setup within current supported mapping scope;
10. 30/60/120/144 fps motion comparison;
11. Intel/AMD/NVIDIA where available;
12. Spotlight disabled idle comparison against the v0.6.0 baseline.

Qualitative acceptance questions:

- Does the viewer immediately know where to look?
- Is the outside region dark enough without looking theatrical?
- Does local pointer motion remain calm in Smart mode?
- Does the Spotlight ever appear to “chase” or oscillate?
- Does it settle at the same moment/intent as the camera?
- Can a beginner enable it without reading documentation?

---

## 13. Implementation sequence

P5 should be implemented in small independently testable slices.

### P5.0 — GUI information architecture only

- regroup current properties into Quick Setup / Controls / Advanced;
- improve beginner-facing labels/tooltips;
- preserve stored values and camera behavior;
- add no Spotlight rendering yet.

**Gate:** zero camera behavior regression.

### P5.1 — Spotlight GPU primitive

- analytic soft mask in existing presentation pass;
- enabled/disabled state;
- size, dim strength, feather, shape;
- Preview Spotlight;
- no pointer/click/Smart behavior yet.

**Gate:** one-pass lightweight rendering, disabled path unchanged.

### P5.2 — Cursor Spotlight

- consume existing mapped cursor coordinates;
- output-space stable size;
- bounded visual smoothing;
- invalid mapping fail-safe.

### P5.3 — Click Spotlight

- capture one content-space click anchor;
- persistent lock and smooth relocation;
- coexist with click rings.

### P5.4 — Smart Focus Spotlight

- expose semantic focus output from the accepted planner through a read-only presentation-effect seam;
- consume it without adding camera authority;
- hold calm during local explanation;
- synchronize visually with TRACK → SETTLE → HOLD.

### P5.5 — GUI polish + release gates

- dynamic visibility;
- recommended defaults/copy;
- final tooltips/localization;
- performance/regression tests;
- direct OBS acceptance matrix;
- update README/CHANGELOG only after feature is actually shipped.

P4.2 deterministic multi-capture target selection may proceed independently. P5 must consume the shared mapping contract so future P4.2 support extends Spotlight naturally instead of requiring a second implementation.

---

## 14. Prohibited shortcuts

Reject an implementation that achieves the visual result by any of the following:

- adding a semi-transparent black OBS source above the scene;
- creating a helper scene item with a mask;
- manipulating user scene-item transforms;
- rendering the scene into a second private texture solely for Spotlight;
- CPU rasterizing a mask every frame;
- generating/updating PNG files;
- using browser-source HTML/CSS as the production effect;
- performing Gaussian blur/multi-pass blur for the default effect;
- letting Spotlight target movement feed camera target movement;
- adding a parallel “Smart Spotlight planner” that reinterprets cursor intent independently of `SceneViewportPlanner`.

If the one-pass analytic approach is insufficient, document the reproducible visual defect and performance consequences before changing architecture.

---

## 15. Definition of done

P5 Spotlight is done only when all are true:

- three modes ship: Smart Focus, Cursor, Click;
- Smart Focus is the recommended default when Spotlight is first enabled;
- existing users remain visually unchanged because Spotlight defaults Off;
- all modes use the shared mapping/camera coordinate model;
- no mode can influence semantic camera motion;
- outside dimming looks soft/premium in direct OBS trials;
- no hard mask edge, blur pass, helper source, scene mutation, or frame readback is introduced;
- Spotlight disabled preserves low idle cost/pass-through behavior;
- GUI common path is simpler than v0.6.0, not more complex;
- advanced camera/mapping controls are still available but not forced on beginners;
- deterministic state/mapping/render math tests pass;
- P4.1 camera regression suite passes unchanged;
- direct OBS acceptance scenarios pass;
- README and release notes describe Spotlight only after implementation has passed these gates.

---

## 16. Product positioning

The intended distinction is not “ArZoom has a dark circle around the cursor.”

The intended product story is:

> **ArZoom acts like a lightweight live presentation director: it frames the right context, moves like a stabilized camera operator, and can guide audience attention with a soft focus spotlight — without rewriting the user’s OBS scene.**

That positioning should guide implementation choices whenever visual novelty conflicts with calmness, reliability, or performance.
