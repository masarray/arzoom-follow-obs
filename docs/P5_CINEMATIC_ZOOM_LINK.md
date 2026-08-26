# P5 — Cinematic Spotlight Linked to Zoom

**Status:** planned extension to P5. Blocked by P0 render-lifecycle issue #24. Do not implement runtime choreography until direct OBS black/flicker acceptance is green.

## 1. User intent

Add one beginner-facing checkbox that links Spotlight presentation intent to the existing Zoom/Follow presenter state.

Recommended label:

**Cinematic Spotlight with Zoom**

Short help text:

> When zoom starts, Spotlight smoothly closes from a full-screen focus into the selected focus area. When zoom ends, it smoothly opens back to full screen and turns off.

This is not a fourth Spotlight behavior mode. Smart Focus / Cursor / Click still decide **where** the Spotlight is centered. Cinematic Zoom Link decides **how Spotlight appears and disappears when zoom is activated/deactivated**.

## 2. Visual target

The intended presentation feel is a restrained cinematic iris / detective-focus shot:

```text
1x / full view
no visible dimming
focus aperture larger than the whole frame
        ↓ Zoom starts
camera begins accepted zoom trajectory
        +
Spotlight aperture smoothly contracts
background dim gently ramps in
        ↓
zoom reaches working shot
Spotlight reaches configured Circle size
background reaches configured dim level
        ↓
normal Smart / Cursor / Click behavior
        ↓ Zoom ends
Spotlight aperture smoothly expands
background dim ramps out
        ↓
full frame is bright again
Spotlight runtime becomes inactive
```

The motion should feel deliberate and satisfying, not like an alarm, searchlight, or game effect.

## 3. Checkbox semantics

New persisted setting proposal:

```text
spotlight_link_to_zoom = false
```

Beginner GUI:

```text
SPOTLIGHT
  Enable Spotlight controls
  Cinematic Spotlight with Zoom [ ]
  Spotlight Mode
  Focus Shape
  Spotlight Area Size
  Background Dim
```

When the checkbox is OFF, existing Toggle Spotlight / Hold Spotlight / Peek behavior remains available and unchanged.

When the checkbox is ON:

- Zoom activation automatically requests Spotlight runtime.
- Spotlight begins from a visually full-frame aperture, not from the configured small Circle.
- It contracts smoothly toward the current Spotlight target while camera zoom progresses.
- Zoom deactivation reverses the reveal and Spotlight becomes runtime-inactive only after the aperture has fully reopened and dim strength has reached zero.
- Manual Hold Spotlight remains an explicit presenter override.
- Manual Toggle Spotlight policy must be deterministic and documented; recommended initial rule is `manual OR auto-link`, so manual presenter intent can keep Spotlight active even after zoom ends.

## 4. Geometry

For a true full-screen start, do not fake this by disabling the mask on frame 0 and suddenly enabling it on frame 1.

Compute a full-frame radius in output pixels that encloses every canvas corner plus feather:

```text
full_radius = 0.5 * sqrt(width^2 + height^2) + feather + safety_margin
```

For Circle mode:

```text
radius(t) = lerp(full_radius, configured_radius, reveal_curve(t))
```

For the zoom-out reverse:

```text
radius(t) = lerp(configured_radius, full_radius, reveal_curve(t))
```

This guarantees the initial/final mask is visually equivalent to no Spotlight regardless of 16:9, ultrawide, portrait, 1080p, 1440p, or 4K output.

## 5. Dimming choreography

Radius contraction should carry most of the visual motion. Background dim should arrive slightly more gently so the viewer never sees a sudden dark flash.

Recommended first trial:

- reveal duration: ~360 ms;
- close radius starts immediately;
- dim ramp begins after ~40–70 ms;
- dim reaches target near the final 20% of the reveal;
- release/open duration: ~280 ms;
- dim begins fading immediately on release;
- aperture expands while dim fades;
- runtime turns fully Off only after dim = 0 and aperture >= full_radius.

Exact timings are trial constants, not public promises until direct OBS acceptance.

## 6. Motion curve

Use the same visual philosophy as ArZoom camera motion: no abrupt velocity discontinuities.

Preferred normalized reveal curve:

```text
minimum_jerk(t) = 10t^3 - 15t^4 + 6t^5
```

This gives zero velocity and acceleration at both ends and should feel more cinematic than linear/smoothstep-only shrinking.

Do not derive this animation from frame count. It must be time-based and equivalent at 30/60/120/144 fps.

## 7. Relationship to camera motion

The dependency direction remains one-way:

```text
Presenter Zoom Intent
        ├──> accepted Camera trajectory
        └──> Spotlight reveal choreography

Shared mapped / semantic focus
        └──> Spotlight center
```

Spotlight reveal progress must never:

- alter camera target;
- alter camera velocity/acceleration;
- wake Smart Follow;
- change safe-zone pressure;
- mutate OBS scene items.

The animation is synchronized presentation choreography, not another camera planner.

## 8. Center behavior during reveal

At reveal start, the aperture is larger than the frame, so its exact center is visually irrelevant. This gives ArZoom a clean opportunity to acquire the intended focus without a visible jump.

Recommended policy:

1. Capture the current valid Smart/Cursor/Click target when Zoom begins.
2. Seed the Spotlight center immediately while aperture is still full-frame.
3. Contract toward that already-valid center.
4. Continue normal mode-specific target behavior after the reveal reaches the working radius.

This avoids the cheap look of a small Circle appearing at screen center and then flying toward the pointer.

## 9. Runtime state

Conceptual bounded state:

```text
CinematicSpotlightLinkState
├─ enabled_setting
├─ phase: Inactive | Closing | Active | Opening
├─ progress 0..1
├─ start_radius_px
├─ target_radius_px
├─ visual_radius_px
├─ visual_dim_strength
└─ last_zoom_requested
```

Requirements:

- O(1) fixed state;
- no animation history;
- no allocation in video tick/render;
- time-based progress;
- deterministic interruption/reversal.

## 10. Mid-animation reversal

Presenter controls may reverse quickly.

Required behavior:

- Zoom ON during Opening: reverse smoothly from the current visual radius/dim, not restart from full screen.
- Zoom OFF during Closing: reverse smoothly from the current visual radius/dim, not snap to configured radius first.
- repeated rapid ON/OFF commands remain bounded and continuous.

The current visual state is always the initial condition for the next transition.

## 11. Interaction with Spotlight modes

### Smart Focus
Recommended cinematic pairing. Center uses the accepted read-only semantic/context target. The aperture contracts into the area ArZoom believes is being explained.

### Cursor
The aperture contracts around the mapped pointer. Pointer smoothing remains visual-only.

### Click
If a valid click anchor exists, contract around it. If no click anchor exists when auto-linked Zoom begins, fail safe: either remain full-frame until the first valid click or use a clearly documented temporary Smart target. Initial recommendation: remain visually full-frame until a valid Click anchor exists; do not guess.

## 12. Interaction with manual Spotlight controls

Recommended v1 arbitration:

```text
runtime_requested =
    master_enabled AND
    (manual_latched OR manual_hold OR gui_peek OR cinematic_zoom_link_active)
```

Manual Hold must always be able to keep Spotlight visible.

Manual Toggle should not be silently cancelled by Zoom ending. If manual latch is ON, Opening stops at the configured working aperture rather than turning Spotlight fully Off.

## 13. GPU/performance contract

This feature changes only Spotlight uniforms/state:

- radius / half-size;
- dim strength;
- center;
- enabled flag.

No additional texture, blur pass, scene render, frame readback, helper source, PNG, or CPU raster mask is allowed.

At steady Active state, cost must be identical in class to ordinary Spotlight.

At Inactive state after Opening completes, existing low-cost/pass-through behavior must be restored.

## 14. P0 dependency

Issue #24 is a hard blocker.

The current direct OBS symptom shows that activating a processed filter frame can still expose black/stale output during first-click and cursor-style transitions. Cinematic Zoom Link intentionally introduces additional controlled transitions between inactive and active Spotlight states, so implementing it before #24 is resolved would multiply the exact lifecycle surface currently under investigation.

Therefore implementation order is:

1. fix #24 and prove zero black/flicker in OBS 32.2.2;
2. preserve the accepted render owner;
3. add Cinematic Zoom Link state only;
4. add deterministic reveal/reversal tests;
5. direct OBS visual tuning;
6. only then expose the checkbox as normal UI.

## 15. Deterministic tests

Required before direct trial:

- full_radius covers all four output corners plus feather;
- t=0 produces effectively full-screen/no-dim output;
- t=1 produces configured working radius + dim;
- close/open are monotonic;
- 30/60/120/144 fps converge to equivalent state;
- mid-close Zoom OFF reverses continuously;
- mid-open Zoom ON reverses continuously;
- manual Hold overrides auto-open completion;
- manual latch survives Zoom OFF;
- Click mode without anchor never guesses;
- animation state cannot modify camera/planner state.

## 16. Direct OBS acceptance

After #24 is green:

1. start at full 1x view;
2. activate Zoom/Follow;
3. verify there is no visible dark pop on the first frame;
4. aperture should close smoothly from outside the frame into the focus Circle;
5. camera and aperture should feel coordinated but not mechanically locked frame-for-frame;
6. release Zoom and verify smooth opening + fade to full bright view;
7. reverse repeatedly mid-animation;
8. test 2x and 4x;
9. test Smart, Cursor, and Click;
10. test 30/60/120/144 fps;
11. zero black/flicker throughout.

## 17. Product principle

The feature should look like a live editor intentionally directing the audience's eyes.

The target feeling is:

> **full scene → cinematic narrowing of attention → calm focused explanation → smooth return to context**

It should be satisfying enough to notice, but restrained enough that users can leave it enabled for long professional tutorials.