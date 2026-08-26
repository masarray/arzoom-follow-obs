# P5 — Cinematic Spotlight Linked to Zoom

**Status:** implemented as the v23 direct-OBS candidate on `feature/p5-spotlight`. P0 Issue #24 is resolved by the v21/v22 full shader-ABI fix. Do not call the cinematic behavior released until direct OBS visual acceptance is complete.

## 1. Product intent

`Cinematic Spotlight with Zoom` links presentation choreography to the existing Zoom intent without creating a fourth Spotlight behavior mode.

- **Smart Focus / Cursor / Click** decide **where** the Spotlight is centered.
- **Cinematic Spotlight with Zoom** decides **how** Spotlight appears and disappears when Zoom activates/deactivates.
- Camera/planner remains the sole camera authority.

Accepted beginner defaults for the current P5 candidate:

- Spotlight Mode: **Follow cursor**
- Focus Shape: **Circle**
- Spotlight Area Size: **170%**
- Background Dim: **35%**
- Edge Softness: **40 px**
- Presentation Cursor: **ArZoom Classic Hand**
- Cinematic Spotlight with Zoom: **On**
- Cinematic Focus Speed: **Balanced**

The master `Enable Spotlight controls` remains **Off by default** so a newly added filter never places an effect on-air without presenter opt-in.

## 2. Visual behavior

```text
full 1x view
aperture already covers the whole frame
background remains fully bright
        ↓ Zoom ON
focus target is acquired while aperture is visually invisible
        ↓
aperture contracts with minimum-jerk motion
background dim follows slightly later
        ↓
working Zoom shot
Circle reaches configured 170% area
background reaches configured 35% dim
        ↓ Zoom OFF
animation reverses from current state
background begins restoring immediately
aperture expands beyond the whole frame
        ↓
Spotlight runtime becomes inactive
true OBS pass-through resumes when no other presentation effect needs the pass
```

The target is a restrained cinematic iris / detective-focus shot, not a searchlight, game effect, hard wipe, or dark flash.

## 3. Runtime implementation (v23)

Production source candidate: `src/arzoom-filter-v23.cpp`.

New persisted settings:

```text
spotlight_link_to_zoom
spotlight_cinematic_speed
```

One WIP migration marker is used only to move existing P5 trial filters to the accepted current defaults:

```text
p5_cinematic_defaults_v1
```

The migration intentionally does **not** modify the master Spotlight enable setting.

`src/arzoom-cinematic-spotlight.hpp` owns bounded time-based choreography:

```text
CinematicSpotlightState
├─ value 0..1
├─ start_value
├─ target_value
├─ elapsed
└─ duration
```

There is no history buffer, per-frame allocation, image analysis, or second semantic planner.

## 4. Motion curve and speed presets

The reveal uses the quintic minimum-jerk curve:

```text
minimum_jerk(t) = 10t^3 - 15t^4 + 6t^5
```

This gives zero velocity and acceleration at both endpoints.

Current engineering trial durations:

| Speed | Close | Open |
| --- | ---: | ---: |
| Smooth | 480 ms | 400 ms |
| Balanced | 360 ms | 300 ms |
| Snappy | 260 ms | 220 ms |

These are tuning constants, not public release promises until direct OBS acceptance.

## 5. Full-screen aperture geometry

The full aperture is derived from the **actual output-space focus center**, not only the canvas center. This is required because a cursor can be near any screen edge.

Conceptually:

```text
max_dx = max(center_x_px, width - center_x_px)
max_dy = max(center_y_px, height - center_y_px)
full_radius = sqrt(max_dx^2 + max_dy^2) + feather + safety_margin
```

Therefore the full state covers every canvas corner at center, edge, or corner focus positions.

During animation the shader receives a scalar cinematic aperture multiplier. The configured 170% working area remains untouched; the multiplier expands that same analytic aperture until it covers the entire frame.

## 6. Dimming choreography

Radius motion leads; dimming follows.

The current v23 dim curve delays the dim by roughly the first 12% of close progress, then applies the same minimum-jerk family to the remaining progress. On opening, because the state reverses continuously, dim begins restoring immediately.

At the endpoints:

```text
full-frame state: cinematic_dim_mix = 0
focused state:    cinematic_dim_mix = 1
```

The user-configured dim value remains 35%; choreography only multiplies it from 0 → 1.

## 7. Mid-animation reversal

Rapid presenter input must never snap or restart.

- Zoom OFF during Closing reverses from the exact current visual state.
- Zoom ON during Opening reverses from the exact current visual state.
- transition duration is scaled by remaining distance.
- repeated rapid ON/OFF stays bounded and continuous.

This is deterministic and time-based rather than frame-count-based.

## 8. Focus acquisition

The focus is seeded while the aperture is still larger than the frame, so target acquisition is visually hidden.

### Cursor
Uses the existing proven mapped pointer with bounded visual smoothing. This is the current default.

### Smart Focus
Uses the existing read-only camera context threshold. It does not write `requested_zoom`, camera target, velocity, acceleration, safe-zone pressure, or scene transforms.

### Click
Uses one content-space click anchor. If no valid anchor exists when Zoom begins, the cinematic aperture remains full-frame rather than guessing a target.

## 9. Manual Spotlight arbitration

Manual presenter intent remains independent.

Manual Toggle / Hold / Peek retain their existing immediate Spotlight behavior. Cinematic Zoom choreography is an additional activation source only when its checkbox is enabled.

Manual presenter controls must never be silently cancelled by Zoom ending.

## 10. Renderer and GPU contract

v23 preserves the accepted v22/v18 render ownership:

```text
camera / click / Presentation Cursor / Spotlight
                ↓
       one shared presentation pass
```

Cinematic mode adds only two scalar shader uniforms:

```text
spotlight_cinematic_scale
spotlight_cinematic_dim_mix
```

Both are initialized to neutral `1.0` on **every processed Draw** before optional cinematic values overwrite them. This explicitly preserves the lesson from P0 Issue #24: no new shader parameter may ever be left unset on D3D11.

Forbidden remains unchanged:

- no second scene render;
- no helper source;
- no scene-item transform mutation;
- no CPU readback;
- no CPU raster mask;
- no PNG/browser overlay;
- no blur/bloom/multi-pass production path;
- no unbounded history.

## 11. Deterministic validation

v23 adds `arzoom-p5-cinematic-spotlight` and keeps the P0 neutral-shader-ABI gate.

Required deterministic checks include:

- minimum-jerk endpoints and monotonicity;
- full radius covers all corners, including edge/corner focus;
- delayed dim begins at zero and converges to one;
- close converges monotonically;
- mid-close reversal is continuous;
- equivalent wall time at 30/144 fps reaches the same endpoint;
- Smooth > Balanced > Snappy duration ordering;
- P0/P4.1 existing regression suite stays green.

Current v23 CI result: 18/18 deterministic tests green, Windows C++/shader compile green, installer/package green.

## 12. Direct OBS acceptance

Before release/merge, test the v23 candidate on the real OBS environment:

1. Verify migrated/default settings: Cursor / Circle / 170% / 35% / 40 px / Classic Hand.
2. Keep master Spotlight disabled and confirm normal ArZoom remains unchanged.
3. Enable Spotlight controls; verify `Cinematic Spotlight with Zoom` is On by default.
4. Zoom ON: no pop/dark flash; aperture should close from outside the frame toward the cursor.
5. Zoom OFF: aperture should open smoothly and background return to full brightness.
6. Reverse Zoom repeatedly mid-animation; no snap/restart.
7. Test Smooth / Balanced / Snappy.
8. Test Smart, Cursor, and Click modes.
9. Test manual Toggle/Hold while Zoom is active and after Zoom ends.
10. Re-run click and Presentation Cursor style stress from P0 acceptance.
11. Confirm zero GUI properties flicker.
12. Upload Current Log and confirm zero `device_draw (D3D11): Not all shader parameters were set` warnings from ArZoom.

## 13. Product principle

The intended feeling is:

> **full scene → cinematic narrowing of attention → calm focused explanation → smooth return to context**

The effect should be noticeable enough to direct attention, but restrained enough to remain enabled throughout a professional tutorial.