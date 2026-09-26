/* SPDX-License-Identifier: GPL-3.0-or-later
 * Portable fixed-time sample -> integer timed-event producer, no hardware I/O.
 * SPSC queue: foreground producer, one consumer. Reset/init only while stopped.
 */
#ifndef EXPERIMENTAL_FTM_H
#define EXPERIMENTAL_FTM_H
#include <stdbool.h>
#include <stdint.h>
#define FTM_AXES 3
#define FTM_QUEUE_CAPACITY 256u

typedef enum { FTM_OK=0, FTM_FULL, FTM_IDLE, FTM_END, FTM_UNDERRUN,
               FTM_INVALID, FTM_RATE_LIMIT, FTM_ABORTED } ftm_status_t;
typedef struct {
    uint32_t ticks;       /* Wait before pulse; direction must be set before wait. */
    uint8_t step_mask, direction_mask, end;
} ftm_event_t;
typedef struct {
    uint32_t sample_ticks, min_event_ticks;
} ftm_config_t;
typedef struct {
    ftm_config_t config;
    ftm_event_t queue[FTM_QUEUE_CAPACITY];
    uint32_t head, tail, fault; /* Atomic accesses: release/acquire publication. */
    uint32_t underruns, high_water, produced, consumed;
    int32_t planned[FTM_AXES];
    uint32_t magnitude[FTM_AXES], accumulator[FTM_AXES];
    uint32_t divisions, index, previous_tick;
    uint8_t direction;
    bool pending, closing, end_queued, running, ended;
} ftm_t;

bool ftm_init(ftm_t *m, const ftm_config_t *config, const int32_t origin[FTM_AXES]);
/* One absolute position in fractional steps for each fixed sample period.
 * FULL is retryable with identical input, all other errors latch until reset. */
ftm_status_t ftm_submit(ftm_t *m, const double position[FTM_AXES]);
/* Pump an accepted sample incrementally without blocking when FIFO fills. */
ftm_status_t ftm_pump(ftm_t *m);
ftm_status_t ftm_finish(ftm_t *m); /* Call only after all filter tails are drained. */
ftm_status_t ftm_start(ftm_t *m);
/* Integer-only consumer. Empty during running latches underrun; never replays.
 * Hardware integration must stop its timer and raise a core alarm on faults.
 * Success hands out one event, not a claim that a physical pulse has executed. */
ftm_status_t ftm_pop(ftm_t *m, ftm_event_t *event);
void ftm_abort(ftm_t *m); /* Atomic latch; caller must also stop physical outputs. */
uint32_t ftm_fill(const ftm_t *m);
#endif
