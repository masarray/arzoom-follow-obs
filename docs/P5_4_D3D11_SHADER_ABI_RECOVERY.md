# P5.4 — D3D11 Shader ABI Recovery

**Status:** evidence-backed P0 candidate. Direct OBS acceptance required before P5 continues.

## Evidence

A direct OBS session on Windows / OBS 32.1.2 with Intel Iris Xe D3D11 produced the decisive warning immediately after the P5 ArZoom filter was created:

```text
device_draw (D3D11): Not all shader parameters were set
```

The warning repeated continuously during processed ArZoom frames.

The user-observed behavior matched this exactly:

- idle/pass-through looked normal;
- click activated the shared effect briefly and produced a black flicker;
- Zoom ON kept the shared effect active and could keep output black;
- Presentation Cursor activity/style changes could keep output black;
- Spotlight ON restored valid output because its active route populated the full Spotlight parameter set before drawing.

## Root cause

P5 extended the existing `Draw` / `PSZoom` technique with these uniforms:

- `spotlight_enabled`
- `spotlight_center`
- `spotlight_half_size_px`
- `spotlight_feather_px`
- `spotlight_dim_strength`
- `spotlight_shape`
- `spotlight_corner_radius_px`
- `spotlight_area_scale`
- `spotlight_circle`

When Spotlight was OFF, the inherited path only guaranteed a disabled flag. The other P5-only parameters were not guaranteed to be initialized before camera/click/cursor activated the same Draw technique. D3D11 therefore rejected the draw as an incomplete shader parameter set.

## Rejected hypotheses

P5.3/v19 bounded warm frames did not change direct OBS behavior and are rejected as the root fix.

P5.4/v20 cursor-sampler prebind was also not the root fix. Its assumption that effect textures had to be bound before `obs_source_process_filter_begin()` was incorrect; normal OBS filter flow is begin -> set effect parameters -> end.

Neither wrapper is part of the production build after this recovery.

## v21 rendering invariant

Production returns to P5.2/v18 single-owner rendering.

Before any frame that actually needs the shared processed Draw technique, v21 writes a complete deterministic neutral Spotlight ABI:

```text
spotlight_enabled          = 0
spotlight_center           = 0.5, 0.5
spotlight_half_size_px     = 1, 1
spotlight_feather_px       = 1
spotlight_dim_strength     = 0
spotlight_shape            = ellipse/0
spotlight_corner_radius_px = 0
spotlight_area_scale       = 1
spotlight_circle           = 1
```

These values are mathematically valid and visually inert. If Spotlight is active, P5.2 then overwrites the neutral packet with the real configured values before the draw ends.

True idle does not gain an always-render cost; normal OBS skip/pass-through remains intact.

`arzoom-p0-p5-neutral-shader-abi` is the deterministic contract gate.

## v22 GUI invariant

Direct feedback also exposed a separate GUI issue: the Properties `Toggle Spotlight` button returned `true`. In OBS a successful button callback returning true asks the Properties dialog to refresh/rebuild. The dynamic runtime status text depended on that rebuild, causing visible slider/widget flicker.

Runtime presenter controls must be UI-layout neutral:

```text
Toggle Spotlight -> atomic runtime state only -> return false
Peek Spotlight   -> atomic request only       -> return false
Hold hotkey       -> atomic runtime state only
```

The rebuild-dependent dynamic ON/OFF text is removed. Configuration controls may still use normal OBS settings updates, but presenter actions may not reconstruct the Properties sheet.

## Direct OBS acceptance

The candidate passes only when a fresh log and visual test show:

1. no ArZoom `device_draw (D3D11): Not all shader parameters were set` warnings;
2. repeated click never produces a black frame;
3. Zoom ON/OFF never produces a black frame;
4. Presentation Cursor presets/style changes never blank the source;
5. Spotlight OFF and ON both use valid processed draws;
6. Toggle Spotlight is no longer a recovery action;
7. Toggle Spotlight does not visually rebuild/flicker the Properties dialog;
8. no filter bypass/delete is required to recover source output;
9. P0-P4.1 deterministic regression gates remain green.

Cinematic Spotlight remains blocked until these acceptance items pass.
