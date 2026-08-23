# Phase 0 Baseline — ArZoom v0.1.4

This report records deterministic behavior of the current Smooth + Smart Follow math before Smart Camera Motion 2.0 is introduced.

The values below come from the Phase 0 replay harness using the existing `src/arzoom-math.hpp` behavior and current Smooth profile constants (`zoom in 0.34 s`, `zoom out 0.30 s`, `pan 0.23 s`, apparent speed limit `1.35`). They are engineering baselines, not UX targets for Phase 1.

## Deterministic trace baseline

| Trace | Max center displacement | Max apparent speed | Camera move frames | Final framing | Edge violation |
|---|---:|---:|---:|---|---:|
| Stationary + hand jitter | `0.000000` | `0.0000` | `0` | `(0.500, 0.500) @ 2x` | `0` |
| Small explanation circle | `0.004671` | `0.0809` | `11` | `(0.500, 0.5047) @ 2x` | `0` |
| Repeated pointing in one UI region | `0.000000` | `0.0000` | `0` | `(0.500, 0.500) @ 2x` | `0` |
| Slow deliberate nearby move | `0.110000` | `0.3661` | `55` | `(0.610, 0.500) @ 2x` | `0` |
| Fast long relocation | `0.250000` | `1.3500` | `41` | `(0.750, 0.500) @ 2x` | `0` |
| Direction reversal | `0.249857` | `1.3500` | `81` | `(0.2501, 0.500) @ 2x` | `0` |
| Edge travel | `0.353543` | `1.3500` | `120` | `(0.750, 0.7500) @ 2x` | `0` |
| Corner approach | `0.353553` | `1.3500` | `51` | `(0.750, 0.750) @ 2x` | `0` |
| Click after long relocation | `0.289006` | `1.3500` | `45` | `(0.750, 0.355) @ 2x` | `0` |
| Rapid click sequence | `0.030000` | `1.2693` | `22` | `(0.530, 0.500) @ 2x` | `0` |
| Zoom while moving | `0.294889` | `1.0470` | `82` | `(0.7944, 0.5167) @ ~2.957x` | `0` |
| Zoom-out near edge | `0.332512` | `1.3500` | `85` | `(0.500, 0.500) @ 1x` | `0` |
| Negative-coordinate monitor path | `0.197671` | `1.3500` | `50` | `(0.6379, 0.6416) @ 2x` | `0` |
| Ultrawide input / mixed-aspect camera path | `0.316686` | `1.3500` | `117` | `(0.7495, 0.3915) @ 2x` | `0` |

Small floating-point variation between compilers is expected; CI assertions use tolerances rather than exact text matching.

## Frame-rate consistency

The same one-second long-relocation scenario is replayed at:

- 30 fps
- 60 fps
- 120 fps
- 144 fps

The suite requires final camera center and zoom to converge within `0.0015` normalized units / zoom ratio from the 30 fps reference, while keeping edge violation at zero.

## Multi-monitor / coordinate mapping baseline

A monitor rectangle with negative desktop coordinates (`left=-1920`, `top=-180`) is normalized without depending on Win32. The test confirms:

- monitor origin maps to normalized `(0, 0)`;
- far corner maps close to `(1, 1)`;
- replayed camera motion remains edge-safe.

An ultrawide `3440x1440` coordinate path is also normalized before camera math to exercise the normalized-coordinate contract independently from a `16:9` OBS canvas/output concept.

## Click-event baseline

Phase 0 does **not** add click visualization. It only establishes typed click events in deterministic traces so Phase 2 can add GPU visuals without changing the trace vocabulary.

The suite verifies that a single relocation click remains one event and a rapid mixed click trace preserves all five events.

## Current baseline limitations

The v0.1.4 motion model is intentionally being preserved in Phase 0, so these remain known limitations for Phase 1 to improve:

- no explicit presenter-intent state machine;
- no separate Observe / Catch-up / Brake / Settle states;
- no acceleration or jerk model;
- speed limiting exists, but long travel can hit the speed cap without a designed ballistic acceleration curve;
- no hysteresis between follow activation and settle thresholds;
- no velocity-aware look-ahead;
- clicks do not yet influence camera urgency;
- the trace harness mirrors production orchestration rather than sharing one extracted production camera class.

## Microbenchmark baseline

`scripts/run-phase0-validation.ps1` runs `arzoom-motion-benchmark` and writes `phase0-benchmark.txt`. GitHub Actions uploads that file as a build artifact.

First Windows reference run: GitHub Actions `windows-2022`, Build Windows run `#8`, commit `a6e002c2391f63a07b60dea99dc7384765ea3583`.

| Trace | Updates | ns/update | Updates/s |
|---|---:|---:|---:|
| Jitter | `450,000` | `56.16` | `17,807,607` |
| Long relocation | `300,000` | `58.71` | `17,033,067` |
| Edge travel | `450,000` | `57.88` | `17,277,788` |
| Zoom-out near edge | `450,000` | `61.04` | `16,382,461` |

These numbers are a seed reference for relative regression tracking, not a hard Phase 0 threshold. Hosted-runner hardware varies, so future comparisons should prefer repeated history and relative deltas over a single absolute number.

Absolute benchmark values are engineering diagnostics only and must not be presented as cross-machine performance claims.
