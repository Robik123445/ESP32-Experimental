/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EXPERIMENTAL_SMOOTHING_H
#define EXPERIMENTAL_SMOOTHING_H
#include <stdbool.h>
#include "ftm.h"
#define SMOOTHING_MAX_WINDOW 64u
typedef struct {
    double history[SMOOTHING_MAX_WINDOW][FTM_AXES];
    unsigned window,cursor;
} smoothing_t;
/* window=1 is exact bypass. All axes use identical nonnegative FIR weights.
 * Straight lines are preserved; corner blending must pass a contour tolerance
 * gate before live use. Drain window-1 repeated endpoint samples. */
bool smoothing_init(smoothing_t *s,unsigned window,const double origin[FTM_AXES]);
bool smoothing_sample(smoothing_t *s,const double in[FTM_AXES],double out[FTM_AXES]);
#endif
