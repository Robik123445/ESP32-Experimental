# grblHAL ESP32-S3 Experimental Motion

<p align="center">
  <strong>Advanced CNC motion-control experiments on ESP32-S3, built around grblHAL.</strong>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/MCU-ESP32--S3-blue" alt="ESP32-S3">
  <img src="https://img.shields.io/badge/base-grblHAL-2ea44f" alt="grblHAL">
  <img src="https://img.shields.io/badge/language-C-555555" alt="C">
  <img src="https://img.shields.io/badge/status-experimental-orange" alt="Experimental">
  <img src="https://img.shields.io/badge/license-GPL--3.0--or--later-blue" alt="GPL-3.0-or-later">
</p>

Experimental **ESP32-S3 CNC firmware based on grblHAL** exploring modern motion-control techniques while keeping grblHAL's CNC parser, protocol and native STEP/DIR hardware path.

The repository focuses on adapting ideas commonly seen in modern 3D-printer firmware to a CNC-oriented grblHAL architecture without replacing the underlying CNC control stack.

> **Current status:** software integration and host-side validation are complete. Initial testing on a real CNC is in progress. Early machine tests show noticeably smoother and quieter motion, but tuning and systematic hardware qualification are still ongoing.

## What this repository adds

| Feature | Experimental implementation |
|---|---|
| Motion profile | Analytic **S-curve / jerk-limited motion** |
| Fixed-time layer | **1 kHz Fixed-Time Motion** position sampling |
| Resonance control | Configurable **ZV input shaping** for X/Y |
| Path filtering | Optional **trajectory smoothing** |
| Planner | Jerk-aware reachability |
| STEP/DIR output | Existing grblHAL **ESP32-S3 timer/RMT HAL** reused |
| Validation | Planner, timing, pulse-count, hold/resume, abort and safety regression tests |

### Core idea

```text
G-code
  ↓
grblHAL planner
  ↓
jerk-aware motion profile
  ↓
1 kHz fixed-time sampling
  ↓
optional ZV input shaping
  ↓
optional trajectory smoothing
  ↓
native grblHAL segment + STEP/DIR pipeline
```

All experimental features default **OFF**, so the baseline behavior remains available for comparison.

## Why this exists

Marlin has pushed several interesting motion-control concepts into widely available embedded hardware, including **Fixed-Time Motion**, **input shaping** and trajectory smoothing.

This project explores what those ideas look like when integrated into **grblHAL on ESP32-S3** while retaining the existing CNC parser, protocol, planner architecture and native hardware abstraction layer.

This is **not Marlin firmware** and not a drop-in port of Marlin's motion stack. The experimental modules are integrated around grblHAL's existing planner / segment / ISR architecture.

## Hardware status

Real-machine testing has started on a CNC using **NEMA23 stepper motors** with **TB6600-class drivers**.

Early observations:

- lower motor noise
- smoother subjective motion
- current tuning feels slower than the previous aggressive grblHAL setup
- acceleration, velocity and motion parameters still need systematic retuning

These are preliminary operator observations, not benchmark claims.

## Quick links

- [Experimental motion integration](docs/EXPERIMENTAL_MOTION.md)
- [Motion architecture audit](docs/MOTION_ARCHITECTURE.md)
- [ZV input shaping](docs/INPUT_SHAPING.md)
- [Benchmarks and acceptance results](docs/BENCHMARKS.md)
- [Marlin inspiration and attribution](docs/MARLIN_INSPIRATION.md)

## Upstream review

To make the work easier to compare with upstream grblHAL, the experimental integration is also mirrored into a dedicated branch of the real `grblHAL/ESP32` fork:

- [grblHAL Discussion #222](https://github.com/grblHAL/ESP32/discussions/222)
- [experimental-motion-s3 review branch](https://github.com/Robik123445/ESP32/tree/experimental-motion-s3)
- [upstream review guide](https://github.com/Robik123445/ESP32/blob/experimental-motion-s3/UPSTREAM_REVIEW.md)

The standalone repository you are reading remains the complete experimental snapshot.

## Marlin inspiration and lineage

The experiment is intentionally inspired by motion-control features documented by the Marlin project:

- Marlin **Fixed-Time Motion (FT_MOTION / M493)**
- Marlin **ZV / advanced input shaping**
- Marlin **FT Motion trajectory smoothing (M494)**
- Marlin's broader use of S-curve / jerk-aware motion ideas

Marlin documents FTM as a fixed-time event-buffer approach originally contributed by **Ulendo**, with a default 1 kHz trajectory-generation frequency and multiple input shapers.

This repository does not claim to reproduce Marlin's complete implementation. In this grblHAL experiment, the live adapter generates foreground position samples and ultimately reuses the existing grblHAL native segment buffer, Bresenham STEP ISR and ESP32-S3 timer/RMT STEP/DIR path.

See [Marlin inspiration and attribution](docs/MARLIN_INSPIRATION.md) for the feature mapping, links and licensing notes.

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
native grblHAL segment buffer
        ↓
Bresenham STEP ISR
        ↓
ESP32-S3 timer / RMT STEP-DIR HAL
```

## Current implementation

### Analytic S-curve / jerk-limited motion

The experimental S-curve path uses analytic constant-jerk phases and jerk-aware planner reachability.

The previous experimental jerk timing defect was repaired. For the documented X100 / Y33 test case, the old branch emitted **0.837344050 s**, below the ideal constant-acceleration minimum of **0.894427191 s**. The repaired path requests approximately **1.001756 s**, or **1.001000 s** with fixed-time sampling.

Implementation:

```text
main/experimental/scurve.c
main/experimental/scurve.h
```

### Fixed-Time Motion for grblHAL

FTM generates foreground position samples on a **1 ms / 1 kHz grid** and feeds the existing native segment and STEP/DIR pipeline.

Unlike a second independent pulse generator, this implementation deliberately reuses the original ESP32-S3 stepper HAL.

Implementation:

```text
main/experimental/ftm.c
main/experimental/ftm.h
main/experimental/sample_motion.c
```

### ZV input shaping

Optional ZV input shaping can be configured independently for X and Y resonance parameters.

The live Cartesian adapter composes a common time kernel across moving axes so XYZ line geometry is preserved before absolute step quantization.

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

Implementation:

```text
main/experimental/shaper.c
main/experimental/shaper.h
```

### Trajectory smoothing

An optional moving-average smoothing stage can be enabled after fixed-time sampling.

Window `1` is bypass.

Implementation:

```text
main/experimental/smoothing.c
main/experimental/smoothing.h
```

## What makes this different from stock grblHAL

This experimental branch adds an optional motion-processing chain between planner output and the native STEP/DIR execution path:

| Area | Baseline grblHAL | Experimental path |
|---|---|---|
| Motion profile | native planner / segment preparation | analytic S-curve or trapezoid |
| Time representation | native variable segment timing | optional 1 kHz fixed-time sampling |
| Resonance control | native path | optional ZV input shaping |
| Smoothing | native path | optional moving-average trajectory smoothing |
| STEP/DIR backend | native ESP32 HAL | **same native ESP32-S3 HAL reused** |
| CNC parser / protocol | grblHAL | **grblHAL retained** |

The experiment is therefore closer to **"Marlin-style motion ideas inside a grblHAL CNC architecture"** than to running Marlin on a CNC controller.

## Clone

```bash
git clone https://github.com/Robik123445/ESP32-Experimental.git
cd ESP32-Experimental
```

The repository is a full development snapshot with vendored dependencies where documented.

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

Physical ESP32-S3 CPU load, worst ISR latency, heap/stack margin and maximum safe real-world STEP frequency still need systematic measurement on hardware.

### Initial physical CNC observations

The firmware has now been exercised on a real production CNC using **NEMA23 stepper motors** and **TB6600-class drivers**.

Early practical observations:

- motor noise is noticeably lower
- motion subjectively feels smoother
- the machine currently feels slower than the previous configuration
- the previous grblHAL setup was tuned aggressively
- acceleration / velocity / motion parameters have **not yet been retuned** for the experimental stack

The "slower" observation is currently operator feedback, not a benchmark result. More controlled physical testing and tuning are required before making performance claims.

## Safety and current limitations

This repository is **experimental CNC firmware**.

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

- [Experimental motion integration](docs/EXPERIMENTAL_MOTION.md)
- [Motion architecture audit](docs/MOTION_ARCHITECTURE.md)
- [ZV input shaping](docs/INPUT_SHAPING.md)
- [Benchmarks and acceptance results](docs/BENCHMARKS.md)
- [Marlin inspiration and attribution](docs/MARLIN_INSPIRATION.md)

## Project context

This firmware is part of **Project Alfa**, an autonomous-workshop development project combining CNC control, machine vision, automation and AI-assisted engineering.

## Upstream, attribution and license

This repository is based on the **grblHAL ESP32 driver** and preserves its open-source licensing and credits.

Upstream grblHAL ESP32:

https://github.com/grblHAL/ESP32

Marlin Firmware:

https://github.com/MarlinFirmware/Marlin

Marlin Fixed-Time Motion documentation:

https://marlinfw.org/docs/features/ft_motion.html

The repository's experimental modules carry `SPDX-License-Identifier: GPL-3.0-or-later`. The included [COPYING](COPYING) file contains the GPLv3 terms and upstream copyright notices, including the existing Marlin notice.

See [docs/MARLIN_INSPIRATION.md](docs/MARLIN_INSPIRATION.md) for additional attribution context.

---

**Search keywords:** grblHAL ESP32-S3, ESP32-S3 GRBL, ESP32S3 CNC, Marlin FTM, Marlin Fixed-Time Motion, grblHAL Fixed-Time Motion, CNC input shaping, ZV input shaping, ESP32-S3 input shaper, S-curve CNC, jerk-limited CNC, trajectory smoothing, STEP/DIR, RMT, ESP-IDF, CNC firmware, motion control, robotics.
