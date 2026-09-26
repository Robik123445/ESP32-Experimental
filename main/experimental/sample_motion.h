/* SPDX-License-Identifier: GPL-3.0-or-later
 * Fixed-grid foreground resampling adapter. No HAL calls, no hardware output.
 * The caller publishes its integer result through the native segment queue. */
#ifndef EXPERIMENTAL_SAMPLE_MOTION_H
#define EXPERIMENTAL_SAMPLE_MOTION_H
#include <stdbool.h>
#include <stdint.h>
#include "shaper.h"
#include "smoothing.h"
#ifndef EXPERIMENTAL_FTM_HZ
#define EXPERIMENTAL_FTM_HZ 1000u
#endif
#ifndef EXPERIMENTAL_FTM_MAX_STEP_HZ
#define EXPERIMENTAL_FTM_MAX_STEP_HZ 100000u
#endif
typedef struct {
    uint32_t steps[3], major;
    uint8_t direction;
    bool done;
} sm_output_t;
typedef struct {
    double origin[3], delta[3], raw[3], filtered[3], velocity[3];
    int32_t last[3];
    unsigned remaining;
    bool draining, enabled;
#if EXPERIMENTAL_INPUT_SHAPING
    shaper_t shape;
#endif
#if EXPERIMENTAL_TRAJECTORY_SMOOTHING
    smoothing_t smooth;
#endif
    uint32_t samples, drains, faults;
} sm_state_t;
/* Configuration shared by normal motion, changed only while idle and drained. */
bool sm_configure(const shaper_config_t *config,unsigned window);
void sm_get_config(shaper_config_t *config,unsigned *window);
bool sm_begin(sm_state_t *s,const int32_t origin[3],const int32_t delta[3],bool filter);
bool sm_sample(sm_state_t *s,double fraction,bool source_done,sm_output_t *out);
#endif
