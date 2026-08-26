# P5.3 — Safe Presentation-Pass Activation Handshake

**Status:** direct-OBS candidate. Blocks further P5 visual/cinematic runtime work until accepted on OBS Studio 32.2.2.

## Why this exists

Direct OBS trials showed a second black-output failure class after the original v0.5.0 cursor-sampler fix:

- first click after idle can flicker the filtered source black;
- changing Presentation Cursor style can leave the filtered source persistently black;
- bypassing/deleting the filter restores Display Capture immediately;
- toggling Spotlight On/Off also restores a valid processed frame.

The recovery caused by Toggle Spotlight is diagnostically important. It indicates that a fresh processed filter frame can repair the pipeline after a bad `skip/pass-through -> processed presentation frame` transition.

P5.2 already removed duplicate camera/click/cursor render ownership, so P5.3 does not add another renderer. It adds a bounded activation handshake in front of the accepted P5.2/P4.1 routing.

## Invariant

Normal ownership remains:

```text
P5.3 handshake gate
        |
        +-- no warm frame --> P5.2 routing
                               |
                               +--> P4.1 presentation renderer

        +-- warm frame ----> neutral existing-effect draw
```

The neutral warm frame does not render click, Presentation Cursor, or Spotlight content.

## Warm-frame triggers

A maximum three-frame handshake is requested on:

1. filter creation;
2. settings/resource update, including Presentation Cursor style/asset changes;
3. first transition from no presentation pass required to a pass required after idle;
4. source/filter reactivation after deactivate.

Requests merge by `max(current, 3)`. They never add together. Repeated UI changes therefore cannot create unbounded work.

## Neutral warm frame

Each warm frame:

- uses the existing ArZoom effect;
- keeps the live accepted camera transform so zoom motion is not reset;
- explicitly disables Spotlight;
- explicitly clears all click uniforms;
- binds the permanent 1x1 transparent cursor fallback directly, independent of whether a newly swapped real atlas appears ready;
- forces Presentation Cursor hidden;
- never locks, reads, or renders the real cursor atlas;
- never changes camera intent;
- never mutates OBS scene items;
- never performs frame readback or creates another scene render graph.

After the bounded warm frames are consumed, normal P5.2/P4.1 rendering resumes.

## Performance contract

This is not an always-render workaround.

At steady idle after the handshake:

```text
no camera + no click + no cursor + no Spotlight
        -> normal OBS filter pass-through/skip
```

The maximum extra work for one activation/update transition is three ordinary analytic effect draws. State is O(1).

## Deterministic gate

`arzoom-presentation-pass-handshake-test` verifies:

- idle -> active edge detection;
- fixed three-frame default;
- warm-frame consumption to exact zero;
- repeated requests merge rather than accumulate;
- no negative pending state.

These tests validate state bounds only. They cannot prove graphics-driver/OBS lifecycle behavior.

## Direct OBS acceptance

Issue #24 remains authoritative. Required on OBS Studio 32.2.2 / Windows:

1. fresh filter, Spotlight Off;
2. 100 repeated left/right/middle clicks after idle;
3. change every built-in Presentation Cursor preset repeatedly;
4. Presentation Cursor Off/On repeatedly;
5. custom asset valid -> invalid -> valid;
6. repeat cursor changes while zoom is active;
7. repeat cursor changes while Spotlight is active;
8. no black flicker and no persistent black frame;
9. Toggle Spotlight must no longer be needed as a recovery action.

If any persistent black output remains, P5.3 is rejected and the lifecycle investigation continues. Do not tune Cinematic Spotlight until this gate is green.

## Cinematic Spotlight dependency

`P5_CINEMATIC_ZOOM_LINK.md` remains planned and intentionally not wired into runtime yet. Its extra inactive/active choreography would increase the exact transition surface under investigation here.
