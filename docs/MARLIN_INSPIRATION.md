# Marlin inspiration and attribution

This repository explores motion-control ideas that are also present in modern **Marlin Firmware**, especially Fixed-Time Motion and input shaping, while integrating them into the **grblHAL ESP32-S3 CNC** architecture.

## Conceptual sources

Useful upstream references:

- Marlin Fixed-Time Motion: https://marlinfw.org/docs/features/ft_motion.html
- Marlin M493 Fixed-Time Motion: https://marlinfw.org/docs/gcode/M493.html
- Marlin Input Shaping: https://marlinfw.org/docs/features/input_shaping.html
- Marlin M593 ZV Input Shaping: https://marlinfw.org/docs/gcode/M593.html
- Marlin M494 FT Motion Trajectory Smoothing: https://marlinfw.org/docs/gcode/M494.html
- Marlin source: https://github.com/MarlinFirmware/Marlin

Marlin documents Fixed-Time Motion as originating from work contributed by **Ulendo**, later extended in Marlin.

## Feature mapping

| Idea | Marlin | This grblHAL experiment |
|---|---|---|
| Fixed-time trajectory generation | FT_MOTION | EXPERIMENTAL_FTM |
| Typical trajectory sample rate | 1 kHz default | 1 kHz |
| Input shaping | ZV and additional FTM shapers | ZV currently |
| Trajectory smoothing | supported in FT Motion | optional moving-average stage |
| Motion execution | Marlin FTM event / step architecture | grblHAL native segment + Bresenham ISR retained |
| Machine focus | primarily 3D printing / general motion | CNC / laser / router experimentation |
| Protocol / parser | Marlin | grblHAL retained |

## Important architectural difference

This repository does **not** replace grblHAL with Marlin.

The live experimental path is:

```text
grblHAL parser
 -> grblHAL planner
 -> optional analytic S-curve
 -> 1 kHz sampled positions
 -> optional ZV shaping
 -> optional smoothing
 -> absolute step quantization
 -> grblHAL native segment buffer
 -> grblHAL Bresenham STEP ISR
 -> ESP32-S3 timer / RMT STEP-DIR
```

That distinction matters because the experiment is specifically about adding modern sampled-motion concepts while preserving grblHAL's CNC behavior and ESP32-S3 hardware abstraction.

## Code provenance and licensing

The experimental modules in `main/experimental/` are marked:

```text
SPDX-License-Identifier: GPL-3.0-or-later
```

The repository includes `COPYING`, which contains the GPLv3 terms and upstream copyright notices, including the Marlin notice already present in the grblHAL ESP32 codebase.

If future work directly copies or adapts additional Marlin source, the relevant original copyright / license notices should remain attached to that code and the source should be clearly attributed here.

## Why document this

The goal is not to blur project ownership. It is to make the engineering lineage obvious:

- **grblHAL** provides the CNC firmware architecture, protocol, planner integration and ESP32 HAL.
- **Marlin / Ulendo work** provides important public reference points for Fixed-Time Motion and input shaping concepts.
- **ESP32-Experimental** is an independent experiment integrating a different sampled-motion path into grblHAL for ESP32-S3 CNC use.

This file should be updated when additional external algorithms or source implementations are incorporated.
