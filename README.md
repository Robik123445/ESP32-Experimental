# ESP32-S3 grblHAL Experimental CNC Firmware

Experimental **ESP32-S3 CNC firmware based on grblHAL**, focused on advanced motion-control development and validation.

This repository contains an independent experimental snapshot of the ESP32 grblHAL driver with software-integrated:

- analytic **S-curve / jerk-limited motion**
- **Fixed-Time Motion (FTM)** at a 1 kHz internal sampling grid
- configurable **ZV input shaping**
- optional **trajectory smoothing**
- ESP32-S3 native **STEP/DIR** output through the existing timer/RMT HAL
- host-side motion, timing, planner, hold/resume and safety regression tests

> **Status:** software integration and host validation are complete. Physical CNC acceptance testing is still pending.

## Why this repository exists

The goal is to explore a safer and more measurable motion-control path for **ESP32-S3 + grblHAL CNC controllers** without modifying the preserved baseline firmware.

The experimental stack keeps the original parser, planner, CNC protocol and native STEP/DIR execution path, while adding optional motion layers that can be compiled independently.

All experimental features default **OFF**.

## Motion pipeline

```text
G-code / grblHAL parser
        ↓
grblHAL planner
        ↓
jerk-aware lookahead
        ↓
analytic S-curve or trapezoid
        ↓
1 kHz fixed-time position sampling
        ↓
optional ZV input shaping
        ↓
optional trajectory smoothing
        ↓
absolute step quantization
        ↓
native segment buffer
        ↓
Bresenham STEP ISR
        ↓
ESP32-S3 timer / RMT STEP-DIR HAL
```

## Current implementation

### S-curve motion

The experimental S-curve path uses analytic constant-jerk phases and jerk-aware planner reachability.

The previous experimental jerk timing defect was repaired. For the documented X100 / Y33 test case, the old branch emitted **0.837344050 s**, below the ideal constant-acceleration minimum of **0.894427191 s**. The repaired path requests approximately **1.001756 s**, or **1.001000 s** with fixed-time sampling.

### Fixed-Time Motion

FTM generates foreground position samples on a **1 ms grid** and feeds the existing native segment and STEP/DIR pipeline.

The implementation deliberately reuses the original ESP32-S3 stepper HAL instead of introducing a second hardware pulse generator.

### ZV input shaping

Optional ZV input shaping can be configured independently for X and Y resonance parameters.

The live Cartesian adapter applies a common time kernel to preserve XYZ line geometry before step quantization.

Persistent grblHAL settings:

```text
$780  X shaper type
$781  X resonance frequency
$782  X damping ratio
$783  Y shaper type
$784  Y resonance frequency
$785  Y damping ratio
$786  smoothing window
```

### Trajectory smoothing

An optional moving-average smoothing stage can be enabled after fixed-time sampling.

Window `1` is bypass.

## Build profiles

Available profiles:

- `baseline`
- `off`
- `scurve`
- `ftm`
- `bench`
- `all`

Typical validation commands:

```bash
./tools/build_motion.sh off
./tools/build_motion.sh all
./tools/build_motion_matrix.sh motion-final
./tools/test_motion.sh
SANITIZE=1 ./tools/test_motion.sh
./tools/benchmark_motion.sh
```

Validated target build environment documented in this repository:

- **ESP-IDF 4.4.6**
- **Xtensa ESP32-S3 GCC 8.4.0**

## Validation status

Current software-side acceptance includes:

- six ESP32-S3 build configurations compiling and linking
- all-off compatibility checks against the preserved baseline
- 1,051 analytic motion profiles
- 120 deterministic randomized multi-block paths
- planner / preparer / ISR endpoint and absolute pulse-count tests
- hold / resume and parking regression tests
- starvation, abort and reset handling
- ZV-only and ZV + smoothing pipeline tests
- ASan / UBSan host test passes

The software STEP harness exercises up to **100,000 steps/s**, but this is **not a claimed physical machine rating**.

Physical ESP32-S3 CPU load, worst ISR latency, heap/stack margin and maximum safe real-world STEP frequency remain to be measured on hardware.

## Safety and current limitations

This repository is **experimental firmware**, not production-ready CNC firmware.

No physical machine acceptance is claimed yet.

Before real machine use, the planned bench sequence includes:

1. ESP32-S3 test with motor power disconnected
2. logic-analyzer verification of STEP/DIR timing
3. preparation and ISR latency measurements
4. heap / stack margin checks
5. USB / streaming load testing
6. limit, probe, hold, reset and safety-door testing
7. controlled low-speed machine motion

Homing, probing, jogging, parking, spindle synchronization and other specialized motion modes retain the native path where documented.

## Documentation

Detailed engineering notes are available here:

- [Experimental motion integration](docs/EXPERIMENTAL_MOTION.md)
- [Motion architecture audit](docs/MOTION_ARCHITECTURE.md)
- [ZV input shaping](docs/INPUT_SHAPING.md)
- [Benchmarks and acceptance results](docs/BENCHMARKS.md)

## Project context

This firmware is part of **Project Alfa**, an ongoing autonomous-workshop development project combining CNC control, machine vision, automation and AI-assisted engineering.

## Upstream and credits

This repository is based on the **grblHAL ESP32 driver** and preserves the original open-source licensing and credits.

Upstream project:

https://github.com/grblHAL/ESP32

See [COPYING](COPYING) for license information.

---

**Keywords:** ESP32-S3, ESP32, grblHAL, GRBL, CNC, CNC controller, motion control, S-curve, jerk-limited motion, fixed-time motion, FTM, input shaping, ZV input shaper, trajectory smoothing, STEP/DIR, RMT, ESP-IDF, robotics, machine control.
