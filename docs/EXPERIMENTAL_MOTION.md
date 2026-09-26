# Experimental motion

## Implemented state — 2026-09-26

The normal three-axis Cartesian pipeline is software-integrated:

```
original parser/CNC commands
 -> grblHAL planner with jerk-aware reachability when S-curve is enabled
 -> analytic S-curve (or independent trapezoid)
 -> foreground 1 kHz fixed-time position samples
 -> optional common ZV time filter
 -> optional common moving-average smoothing
 -> absolute step quantization
 -> native segment buffer and Bresenham STEP ISR
 -> original ESP32-S3 timer / RMT STEP-DIR HAL
```

Each feature defaults OFF. Six build profiles are baseline, off, scurve, ftm,
bench and all. Despite its historical name, **bench now includes live FTM** with
S-curve OFF; `$EMBENCH` itself remains a synthetic no-output benchmark. `all`
enables every layer and diagnostics, but resonance filters default OFF/window 1.
All-off object disassembly and relocations (with compiler-generated local label suffixes normalized) are compared against the immutable
original tag. No file in `/home/robert/ESP32` was modified; no firmware was flashed.

## Exact cause and repair of the jerk defect

The old branch planned ramp distances using an effective constant acceleration
and then integrated changing acceleration against those incompatible boundaries.
At boundaries it snapped computed velocity to planned peak/exit and replaced
elapsed time with the constant-acceleration identity `2*distance/(v0+v1)`.
For X100/Y33 at F30000, A=500 mm/s² and J=5000 mm/s³:

- acceleration boundary: integrated speed 214.294 mm/s, forced to 223.970;
- braking boundary: 76.398 mm/s forced to zero with 0.642361 mm remaining;
- emitted timing 0.837344050 s violated even the ideal constant-A minimum
  `2*sqrt(100/500) = 0.894427191 s`.

`main/experimental/scurve.[ch]` now integrates constant-jerk phases exactly.
For speed change dv: `tj=min(A/J,sqrt(abs(dv)/J))`,
`T=tj+abs(dv)/(J*tj)` and `distance=(v0+v1)*T/2` (dv=0 handled separately).
A bounded bisection finds a feasible peak. Each phase evaluates
`x=x0+v0*t+a0*t²/2+j*t³/6`, `v=v0+a0*t+j*t²/2`, `a=a0+j*t`.
Only arithmetic roundoff is removed at phase boundaries. No artificial time
factor or relaxed safety assertion was introduced.

`planner.c` uses the same transition-distance integral in reverse and forward
lookahead. The old optimized constant-A breakpoints cannot be reused: jerk-aware
reachability is not additive in squared speed. The repaired branch traverses the
queue and rounds stored float speeds down until the speed actually reconstructed
with `sqrtf` is reachable. Once a profile has been prepared, late G-code cannot
change its committed exit velocity. Hold/override replans still take priority.
Bare legacy `ENABLE_JERK_ACCELERATION` without `EXPERIMENTAL_S_CURVE` is rejected
at compilation in this repository; the unsafe alternative cannot be enabled
accidentally. The immutable baseline remains unchanged for comparison.

The unchanged gate passes: S-curve native segments request 1.001756 s; S-curve
plus FTM requests 1.001000 s; the continuous analytic profile is 1.000000 s.

## Integration ownership and safety contracts

`stepper.c:st_prep_buffer` performs all profile and filter math. `sample_motion.c`
rounds absolute coordinates, avoiding incremental rounding drift. Samples own
integer XYZ counts in a ten-slot ring. Native segment publication uses release/
acquire fences with FTM enabled. The ISR only consumes integer counts and
precomputed timer remainders. Every sample's subdivisions sum to exactly 1 ms;
zero-step intervals keep their time. There are nine usable queued samples.

`driver.c:experimental_step_min_period` exposes the actual driver's STEP pulse
period minimum. The adapter reduces optional AMASS subdivisions if needed and
rejects intervals that the driver would otherwise silently clamp. The supported
backend is the original ESP32-S3 timer/RMT; I2S period quantization and non-Cartesian
or non-three-axis configurations are rejected. The default maximum sample output
is 100 steps/ms (100 kstep/s); this is a software bound, not a hardware rating.

FTM uses rest-to-rest boundaries for **every planner block**. Filters finish their
finite tails before block discard. Common X/Y ZV convolution preserves the XYZ
line before step rounding; corners are exact. No blending tolerance is invented.
This conservatism substantially slows tiny lines and tessellated arcs. With FTM
OFF, S-curve retains grblHAL junction speed constraints; a mathematical sharp
corner does not imply globally bounded vector acceleration.

Homing, probing, jog, backlash, system/parking motion, spindle synchronization,
units/revolution, dynamic laser/PWM, CSS and blocks carrying output/message
metadata bypass FTM. Their original callbacks and execution metadata remain in
the native path. S-curve emergency/hold/override braking retains bounded native
trapezoidal deceleration; acceleration may jump there, so jerk is not globally
bounded. The host verifies hold/resume and parking out/back/resume, including
filter state. Full CNC protocol/peripheral behavior still requires bench testing.

Producer starvation stops the timer and raises existing `Alarm_AbortCycle`.
Invalid samples/timing also abort; there is no mid-motion unshaped fallback or
replay of stale events. Fast cancel of sampled normal motion raises abort rather
than rewinding independently filtered positions. Reset clears sample state.
Physical loss of position from such abrupt stops remains possible. Feed hold
brakes the source then drains the configured tail, adding finite stop latency
(up to 574 ms for the largest accepted ZV+smoothing configuration).

## Flags and configuration

| Flag | Behavior |
|---|---|
| EXPERIMENTAL_S_CURVE | Analytic jerk phases, jerk-aware lookahead; existing $800/$801/$802 axis jerk settings |
| EXPERIMENTAL_FTM | 1 kHz internal adapter, independent of S-curve; native STEP HAL |
| EXPERIMENTAL_INPUT_SHAPING | ZV filters; requires FTM; default bypass |
| EXPERIMENTAL_TRAJECTORY_SMOOTHING | Common moving average, window 1..64; requires FTM; default 1 |
| EXPERIMENTAL_MOTION_DIAGNOSTICS | Foreground trace and idle-only commands |

Persistent controller settings `$780`–`$786` list the X/Y ZV types, resonance
frequencies, damping and common smoothing window in `$$`. They are validated and
saved to NVS while Idle. If NVS allocation is unavailable, these settings are not
registered and `$EMCONFIG` refuses writes. `$EMCONFIG` remains a convenience that writes the same
persistent values. Compile FTM, shaping, smoothing and diagnostics first; settings
cannot activate an omitted feature. See
[INPUT_SHAPING.md](INPUT_SHAPING.md) for mathematics and geometry policy.
USB/serial G-code streaming remains the source of commands; no SD dependency.

## Diagnostics and validation

`$EM` reports live underruns, faults, buffer fill, last-block sample/tail counts,
state bytes, foreground preparation total/max time and filtered XYZ step speeds.
Timing is available only when `hal.get_micros` exists. It is not overall CPU load.
`$EMTRACE` prints the bounded last-64 foreground records: planned rate (mm/min),
estimated acceleration (mm/s²), shaped XYZ speeds (steps/s), fill and subdivisions.
No printing, filtering, allocation or clock sampling is added to STEP ISR.
`$EMBENCH` runs an independent portable event-queue reference without GPIO; its
counters are explicitly separate from live FTM counters.

Tests include 1,051 analytic profiles, independent numerical integration and
continuity/bounds checks, 120 deterministic randomized multi-block paths, short/
long/low/high feed moves, nonzero entry/exit, tiny junctions, reversals, acceleration
extremes, streaming, overrides, hold, parking, exact timer sums, pulse totals,
underrun injection, abort/reset, invalid configuration and driver-period rejection.
ZV is tested through actual planner/preparer/STEP with S-curve enabled and disabled.
ASan/UBSan and six ESP32-S3 compile/link configurations are part of verification.

## Remaining work before physical acceptance

Measure real target preparation worst-case latency, ISR execution, CPU/heap/stack
use and buffer margin under streaming and WiFi load. Nine ms is a tight buffer;
there is no demonstrated target scheduling margin yet. Validate DIR setup/hold,
STEP pulse width, cancellation, limits, probe, spindle modes and safety-door
parking on an isolated instrumented bench with motor power disconnected before
any CNC motion. Do not infer physical safety from host timings or compilation.
Dense-path throughput, nonzero-speed FTM junction blending, durable filter settings,
and additional shapers remain future work. The portable `ftm.c` event FIFO remains
a comparison/benchmark module; the live adapter uses the native segment queue.
