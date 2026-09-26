/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EXPERIMENTAL_SHAPER_H
#define EXPERIMENTAL_SHAPER_H
#include <stdbool.h>
#include <stdint.h>
#include "ftm.h"
#define SHAPER_HISTORY 512u
#define SHAPER_MAX_IMPULSES 5u
/* Future kernels need only implement impulse construction, not the FIR engine. */
typedef enum { SHAPER_OFF=0, SHAPER_ZV=1, SHAPER_ZVD=2, SHAPER_EI=3, SHAPER_2HEI=4 } shaper_type_t;
typedef struct { shaper_type_t type; double frequency_hz, damping; } shaper_axis_config_t;
typedef struct { double sample_hz; shaper_axis_config_t axis[FTM_AXES]; } shaper_config_t;
typedef struct { double delay,weight; } shaper_impulse_t;
typedef struct { unsigned count; shaper_impulse_t impulse[SHAPER_MAX_IMPULSES]; } shaper_kernel_t;
typedef struct {
    shaper_config_t config;
    shaper_kernel_t kernel[FTM_AXES];
    double history[FTM_AXES][SHAPER_HISTORY];
    unsigned cursor, tail_samples;
} shaper_t;
bool shaper_init(shaper_t *s,const shaper_config_t *c,const double origin[FTM_AXES]);
/* Foreground only. Absolute positions in steps. Continuous across planner
 * blocks. Drain tail_samples copies of FINAL position at stream end. */
bool shaper_sample(shaper_t *s,const double input[FTM_AXES],double output[FTM_AXES]);
#endif
