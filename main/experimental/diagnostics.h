/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef EXPERIMENTAL_DIAGNOSTICS_H
#define EXPERIMENTAL_DIAGNOSTICS_H
#include <stdint.h>
/* Foreground only, no stream I/O. Never call from STEP ISR. */
void em_trace_reset(void);
void em_trace_shaped(const double velocity[3]);
void em_trace_segment(float velocity_mm_min,float duration_min,uint32_t fill,uint32_t steps);
#if EXPERIMENTAL_FTM
/* Snapshot only while idle; no printing, sampling or clocks in STEP ISR. */
typedef struct {
    uint64_t prep_us;
    uint32_t prep_max_us,prep_calls,samples,drains,faults,underruns,state_bytes,fill;
    double velocity[3];
} em_live_stats_t;
void st_motion_diagnostics(em_live_stats_t *out);
#endif
#endif
