# P5.2 — Render Ownership Recovery

**Status:** P0 recovery after direct OBS trial #2. Draft until direct OBS acceptance.

## Failure observed

On OBS Studio 32.2.2 / Windows, the second P5 trial exposed a severe regression while an ArZoom filter was attached to Display Capture:

- clicking the captured display could flicker the filter output to blank/black;
- changing the Presentation Cursor/icon could leave the entire filtered Display Capture permanently black;
- disabling or deleting the ArZoom filter immediately restored the underlying Display Capture.

This is a filter-render regression, not a Display Capture source failure.

## Root cause class

P4.1 already had one accepted owner for the shared presentation pass:

```text
phase41_cursor_scaled_render()
  -> prime permanent transparent cursor sampler
  -> phase35_render()
       -> camera transform
       -> click uniforms
       -> Presentation Cursor atlas/resource ownership
       -> process_filter_end()
```

The first P5 implementation added `phase5_render()` and later `phase51_render()` that duplicated most of this pass so Spotlight could participate at 1x. That duplicated responsibility for:

- `obs_source_process_filter_begin/end`;
- cursor atlas readiness and resource locking;
- cursor visibility/sampler state;
- click uniform clearing/population;
- camera + viewport uniforms.

CI could compile and deterministic math tests could pass while direct OBS lifecycle transitions still failed. Changing a cursor asset is exactly the kind of asynchronous resource transition that exposed this duplicated ownership.

## Permanent architecture rule

**Spotlight must never duplicate the complete camera/click/Presentation-Cursor renderer.**

P5.2 routing is:

```text
Spotlight OFF
  -> exact inherited P4.1 renderer

Spotlight ON + camera/click/renderable cursor already needs presentation pass
  -> publish Spotlight uniforms only
  -> exact inherited P4.1 renderer owns the frame

Spotlight ON and Spotlight is the only visible presentation effect
  -> minimal Spotlight-only pass
     - permanent transparent cursor fallback is mandatory
     - no real cursor atlas ownership
     - cursor forced hidden
     - click uniforms explicitly cleared
     - camera/viewport + Spotlight uniforms only
```

The minimal Spotlight-only pass exists only because the inherited renderer correctly skips GPU work when camera/click/cursor are all visually inactive. It must not grow into a second shared renderer.

## Fail-safe rules

- If the permanent transparent cursor fallback texture is unavailable, a Spotlight-only frame is skipped and the source remains pass-through.
- P5 never swaps, destroys, or substitutes the real Presentation Cursor atlas.
- P5 never holds the Presentation Cursor asset mutex while calling the inherited renderer.
- Cursor asset creation/change remains owned by the accepted Presentation Cursor runtime.
- A P5 state or shader failure disables Spotlight rather than risking the source output.
- Spotlight OFF must remain structurally equivalent to P4.1 rendering, not merely visually similar in normal cases.

## Deterministic gate

`spotlight_needs_solo_pass()` is the routing contract:

- active Spotlight + no camera + no click + no renderable cursor -> solo pass;
- camera active -> inherited pass;
- click active -> inherited pass;
- renderable cursor active -> inherited pass;
- Spotlight inactive -> inherited/pass-through P4.1 behavior.

## Direct OBS acceptance — trial #3

Do not leave Draft until all are confirmed on the real Display Capture workflow:

1. fresh OBS launch with filter active and Spotlight not on-air;
2. repeated left/right/middle clicks — zero black/blank flicker;
3. enable/disable click visualization while clicking — zero black/blank output;
4. change Presentation Cursor preset repeatedly while 1x and while zoomed;
5. enable/disable Presentation Cursor repeatedly;
6. switch from no cursor to built-in cursor and back;
7. perform the same cursor changes while Spotlight is ON;
8. Toggle Spotlight repeatedly at 1x;
9. Hold Spotlight press/release repeatedly;
10. Smart/Cursor/Click Spotlight mode changes;
11. zoom 1x -> 2x -> 4x while Spotlight is ON;
12. disable Spotlight and verify the inherited P4.1 path remains stable;
13. disable/delete the filter should not be required to recover video at any point.

Any persistent black frame is P0 and blocks all visual tuning/merging.

## CI blind spot noted

The current build dependency baseline is OBS 31.1.1, while the failing direct trial was performed on OBS 32.2.2. ABI compatibility is expected for normal plugin use, but direct render-lifecycle acceptance on the current public OBS generation is now required for P5. Future CI should consider adding an additional current-OBS compatibility build/smoke lane rather than treating compilation against the frozen baseline as sufficient runtime evidence.
