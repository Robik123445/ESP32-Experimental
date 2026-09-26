/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "ftm.h"
#include <limits.h>
#include <math.h>
#include <string.h>

static uint32_t acquire(const uint32_t *p) { return __atomic_load_n(p,__ATOMIC_ACQUIRE); }
static void release(uint32_t *p,uint32_t v) { __atomic_store_n(p,v,__ATOMIC_RELEASE); }
static ftm_status_t fault(ftm_t *m,ftm_status_t s) {
    uint32_t expected=FTM_OK;
    __atomic_compare_exchange_n(&m->fault,&expected,s,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
    return (ftm_status_t)acquire(&m->fault);
}
uint32_t ftm_fill(const ftm_t *m) { return acquire(&m->head)-acquire(&m->tail); }
bool ftm_init(ftm_t *m,const ftm_config_t *c,const int32_t origin[FTM_AXES]) {
    if(!m || !c || !origin || !c->min_event_ticks || c->sample_ticks<c->min_event_ticks)
        return false;
    memset(m,0,sizeof(*m));m->config=*c;memcpy(m->planned,origin,sizeof(m->planned));return true;
}
static bool push(ftm_t *m,ftm_event_t e) {
    uint32_t h=acquire(&m->head), n=h-acquire(&m->tail);
    if(n>=FTM_QUEUE_CAPACITY)return false;
    m->queue[h%FTM_QUEUE_CAPACITY]=e;release(&m->head,h+1);
    if(n+1>m->high_water)m->high_water=n+1;
    m->produced++;return true;
}
ftm_status_t ftm_submit(ftm_t *m,const double p[FTM_AXES]) {
    if(acquire(&m->fault))return (ftm_status_t)acquire(&m->fault);
    if(m->closing || !p)return fault(m,FTM_INVALID);
    if(m->pending)return FTM_FULL;
    int32_t target[FTM_AXES];uint32_t count[FTM_AXES],divisions=0;uint8_t direction=0;
    for(unsigned a=0;a<FTM_AXES;a++) {
        if(!isfinite(p[a]) || p[a]<(double)INT32_MIN || p[a]>(double)INT32_MAX)
            return fault(m,FTM_INVALID);
        target[a]=(int32_t)llround(p[a]);
        int64_t d=(int64_t)target[a]-m->planned[a];
        count[a]=(uint32_t)(d<0?-d:d);
        if(d<0)direction|=1u<<a;
        if(count[a]>divisions)divisions=count[a];
    }
    if(!divisions)divisions=1; /* Explicit wait event for a zero-step sample. */
    if(divisions>m->config.sample_ticks/m->config.min_event_ticks)return fault(m,FTM_RATE_LIMIT);
    memcpy(m->planned,target,sizeof(target));memcpy(m->magnitude,count,sizeof(count));
    memset(m->accumulator,0,sizeof(m->accumulator));m->divisions=divisions;
    m->index=m->previous_tick=0;m->direction=direction;m->pending=true;
    return FTM_OK;
}
ftm_status_t ftm_pump(ftm_t *m) {
    if(acquire(&m->fault))return (ftm_status_t)acquire(&m->fault);
    while(m->pending) {
        if(ftm_fill(m)>=FTM_QUEUE_CAPACITY)return FTM_FULL;
        /* 64-bit product avoids overflow for long sample periods. */
        uint32_t t=(uint32_t)((uint64_t)(m->index+1)*m->config.sample_ticks/m->divisions);
        ftm_event_t e={.ticks=t-m->previous_tick,.direction_mask=m->direction};
        for(unsigned a=0;a<FTM_AXES;a++) {
            uint64_t acc=(uint64_t)m->accumulator[a]+m->magnitude[a];
            if(acc>=m->divisions){e.step_mask|=1u<<a;acc-=m->divisions;}
            m->accumulator[a]=(uint32_t)acc;
        }
        /* Sole producer, so space cannot disappear after the capacity check. */
        if(!push(m,e))return fault(m,FTM_INVALID);
        m->previous_tick=t;m->pending=++m->index<m->divisions;
    }
    if(m->closing && !m->end_queued) {
        if(!push(m,(ftm_event_t){.end=1}))return FTM_FULL;
        m->end_queued=true;
    }
    return FTM_OK;
}
ftm_status_t ftm_finish(ftm_t *m) { m->closing=true;return ftm_pump(m); }
ftm_status_t ftm_start(ftm_t *m) {
    if(acquire(&m->fault))return (ftm_status_t)acquire(&m->fault);
    if(m->running || m->ended)return FTM_INVALID;
    if(!ftm_fill(m))return FTM_IDLE;
    m->running=true;return FTM_OK;
}
ftm_status_t ftm_pop(ftm_t *m,ftm_event_t *e) {
    if(!e)return fault(m,FTM_INVALID);
    *e=(ftm_event_t){0};
    if(acquire(&m->fault))return (ftm_status_t)acquire(&m->fault);
    if(m->ended)return FTM_END;
    if(!m->running)return FTM_IDLE;
    uint32_t t=acquire(&m->tail);
    if(t==acquire(&m->head)){m->underruns++;return fault(m,FTM_UNDERRUN);}
    *e=m->queue[t%FTM_QUEUE_CAPACITY];release(&m->tail,t+1);m->consumed++;
    if(e->end){m->running=false;m->ended=true;return FTM_END;}
    return FTM_OK;
}
void ftm_abort(ftm_t *m) { (void)fault(m,FTM_ABORTED); }
