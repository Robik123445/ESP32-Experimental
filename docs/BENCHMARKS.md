# Benchmarks and acceptance results — 2026-09-26

No flashing, serial connection or physical machine motion was performed.
Original baseline: `dcc2b3de27e3d813959861dd05be12f7739651f5` / `original-baseline`.
Current reproducible results: [motion-final evidence](evidence/motion-final/).
Earlier evidence directories preserve the original failure and each integration stage.

## Acceptance

- Original project: all 4,501 files including Git metadata unchanged.
- Six ESP32-S3 build configurations compile/link successfully.
- All-off disassembly **including relocations** (with GCC-generated local label suffixes normalized) is identical to baseline for
  planner, stepper, G-code, protocol, motion_control, state_machine and ESP32 driver.
- The original acceleration gate is unchanged and passes. Old jerk timing
  0.837344050 s was below the 0.894427191 s physical minimum; repaired S-curve
  requests 1.001756 s, or 1.001000 s with fixed-time sampling.
- 1,051 analytic profiles and 120 deterministic randomized multi-block paths pass.
- Actual planner/preparer/ISR endpoints **and absolute pulse totals** pass across
  short/long/low/high feed, entry/exit changes, junctions, reversals, tiny segments,
  acceleration extremes, streaming, override, hold/resume and parking return.
- FTM tests exact timer-tick sums, explicit producer-starvation alarm, fast abort,
  reset, native jog/homing/probe bypass, invalid coordinate counts and rejection
  of periods below the driver's configured minimum.
- ZV-only and ZV+smoothing pass the actual pipeline with S-curve ON and OFF;
  continuous geometry, velocity/acceleration, endpoint and quantization checks pass.
- ASan/UBSan pass the complete suite, plus focused checks of the last timer/range
  guards. An initial test-fixture use-after-free was corrected; it was caused by
  freeing the planner between repeated suites while retaining its internal pointers.
- Independent diagnostics/filter macro combinations compile with `-Wall -Wextra -Werror`.

## Target build sizes

ESP-IDF 4.4.6, Xtensa ESP32-S3 GCC 8.4.0, original sdkconfig. Static data excludes
heap-allocated planner blocks, task stacks and runtime network allocations.

| Profile | Firmware bytes | IRAM bytes | Static data RAM bytes |
|---|---:|---:|---:|
| baseline | 841,824 | 72,646 | 54,029 |
| off | 841,824 | 72,646 | 54,029 |
| scurve | 847,824 | 72,646 | 54,469 |
| ftm | 847,856 | 72,782 | 55,253 |
| bench | 856,752 | 72,782 | 87,949 |
| all | 859,024 | 72,782 | 87,981 |

`all` adds 33,952 static data bytes and 136 IRAM bytes over baseline. It contains
both live filter state, persistent `$` configuration descriptors and the separate optional no-GPIO benchmark state. The
historically named `bench` profile now has live FTM/filter capability, S-curve OFF.
No new compiler warning class was introduced. Earlier full rebuilds emit the
existing spindle warning (`Selected spindle is not fully supported - no direction
output!`); later incremental logs can have no repeated warnings. None was suppressed.

## Host timing comparison

Measured by a fake-HAL harness, not target CPU utilization. A preparation call is
made after each simulated ISR, including many calls when the queue is already full.
Reported totals therefore depend on this deterministic host scheduler. They are
useful regression measurements, not estimates of ESP32 task load or deadline margin.

| Profile, 10 m move | Simulated motion s | Host preparation ms | Host planner µs | Host ISR ms | Segments | Requested max steps/s |
|---|---:|---:|---:|---:|---:|---:|
| baseline | 21.002616 | 60.147 | 0.180 | 68.829 | 2100 | 100000.000 |
| S-curve | 21.107863 | 63.406 | 0.350 | 69.638 | 2108 | 100000.000 |
| FTM | 21.001000 | 63.439 | 0.200 | 72.451 | 21000 | 100000.000 |
| S-curve + FTM | 21.101000 | 62.035 | 0.270 | 66.289 | 21100 | 100000.000 |
| S-curve + FTM + ZV + smoothing | 21.121000 | 65.075 | 0.250 | 73.575 | 21120 | 100000.000 |

All five execute exactly 2,000,000 X steps in this case. Filtered results use
X=40 Hz/z=0.1, Y=63 Hz/z=0.12 and smoothing window=8; the preceding pass uses
window=1. Peak queue occupancy is nine segments. Exact-stop FTM intentionally
reduces dense-path throughput: 30 consecutive one-step blocks take about 0.961 s
with S-curve+FTM and 1.561 s with the configured filters. Their endpoints still match.

The independent portable benchmark processes 500,000 samples with no output:

- FTM_BYPASS: 45.729 host ns/sample, worst batch 67.861 ns/sample; FIFO high-water 1, injected-scheduler underruns 0.
- FTM_ZV_SMOOTHING: 86.236 host ns/sample, worst batch 105.411 ns/sample; FIFO high-water 1, injected-scheduler underruns 0.

Its one-entry high-water and zero underruns belong to a synchronous portable
producer/consumer, **not live ESP32 scheduling evidence**.

## Frequency, CPU, memory and underrun limits

- **100,000 steps/s** is exercised in the software STEP harness. It is also the
  default FTM cap. Maximum safe physical frequency remains **unmeasured**.
- **ESP32-S3 CPU load, worst ISR latency, dynamic heap and stack use: unmeasured**.
  `$EM` now exposes foreground preparation total/max microseconds when the HAL clock
  exists; measuring it on target is a remaining bench task. It excludes ISR/other tasks.
- Static target RAM is measured above. FIR state is bounded, with no allocation
  or logging introduced in the STEP ISR. Planner still allocates its native buffer.
- FTM has nine usable 1 ms slots, so **at most about 9 ms queued coverage**. Under
  producer stalls longer than the remaining coverage, it alarms/stops. Fault
  injection verifies this behavior; it does not establish a sufficiently low
  real-world underrun probability. No WiFi/USB stress margin is claimed.
- Feed hold must drain finite filter delay in addition to normal braking and
  already queued motion. Maximum accepted filter tail is 574 ms; use much shorter
  measured kernels for any later bench evaluation. Abrupt abort can lose position.

## Reproduction and next safe step

```
./tools/build_motion_matrix.sh motion-final
./tools/test_motion.sh
SANITIZE=1 ./tools/test_motion.sh
./tools/benchmark_motion.sh
python3 tools/verify_motion_compatibility.py
python3 tools/collect_motion_stage.py motion-final
```

Never rebuild copied old build caches: they retain original absolute paths. These
helpers use fresh isolated directories and never upload firmware. Before the first
physical CNC test, independently review the motion/stop transitions, then run an
instrumented ESP32 bench with motor power disconnected and a logic analyzer.
Measure STEP/DIR timing, preparation/ISR latency, heap/stack margin, streaming-load
underruns, limits/probe, spindle modes, reset, hold and safety-door parking. No such
hardware action is authorized or performed in this task.
