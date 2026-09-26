/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "sample_motion.h"
#include <limits.h>
#include <math.h>
#include <string.h>
static shaper_config_t configuration={.sample_hz=EXPERIMENTAL_FTM_HZ};
static unsigned smoothing_window=1;
bool sm_configure(const shaper_config_t *c,unsigned w) {
    if(!c || c->sample_hz!=EXPERIMENTAL_FTM_HZ || w<1 || w>SMOOTHING_MAX_WINDOW)return false;
    double total_delay=0;
    if(c->axis[2].type!=SHAPER_OFF)return false; /* live configuration is X/Y */
    for(unsigned a=0;a<3;a++) {
        if(c->axis[a].type==SHAPER_OFF)continue;
#if EXPERIMENTAL_INPUT_SHAPING
        double f=c->axis[a].frequency_hz,z=c->axis[a].damping;
        if(c->axis[a].type!=SHAPER_ZV || !isfinite(f) || f<=0 || f>=c->sample_hz/2 || !isfinite(z) || z<0 || z>=1 || c->sample_hz/(2*f*sqrt(1-z*z))>SHAPER_HISTORY-1)return false;
        total_delay+=c->sample_hz/(2*f*sqrt(1-z*z));
#else
        return false;
#endif
    }
#if !EXPERIMENTAL_TRAJECTORY_SMOOTHING
    if(w!=1)return false;
#endif
    if(total_delay>SHAPER_HISTORY-1)return false;
    configuration=*c;smoothing_window=w;return true;
}
void sm_get_config(shaper_config_t *c,unsigned *w) { *c=configuration;*w=smoothing_window; }
bool sm_begin(sm_state_t *s,const int32_t origin[3],const int32_t delta[3],bool filter) {
    memset(s,0,sizeof(*s));s->enabled=filter;
    for(unsigned a=0;a<3;a++){s->origin[a]=s->raw[a]=s->filtered[a]=s->last[a]=origin[a];s->delta[a]=delta[a];}
#if EXPERIMENTAL_INPUT_SHAPING
    shaper_config_t c=configuration;if(!filter)for(unsigned a=0;a<3;a++)c.axis[a].type=SHAPER_OFF;
    if(!shaper_init(&s->shape,&c,s->origin))return false;
    /* A common positive impulse kernel preserves the straight CNC path.
       Compose X/Y ZV kernels for moving axes; do not independently delay XYZ. */
    shaper_kernel_t common={.count=1,.impulse={{0,1}}};
    for(unsigned a=0;a<2;a++)if(delta[a] && c.axis[a].type!=SHAPER_OFF) {
        shaper_kernel_t next={0};
        for(unsigned i=0;i<common.count;i++)for(unsigned j=0;j<s->shape.kernel[a].count;j++) {
            shaper_impulse_t x=common.impulse[i],y=s->shape.kernel[a].impulse[j];
            if(next.count==SHAPER_MAX_IMPULSES)return false;
            next.impulse[next.count++]=(shaper_impulse_t){x.delay+y.delay,x.weight*y.weight};
        }
        common=next;
    }
    double delay=0;for(unsigned i=0;i<common.count;i++)delay=fmax(delay,common.impulse[i].delay);
    if(delay>SHAPER_HISTORY-1)return false;
    for(unsigned a=0;a<3;a++)s->shape.kernel[a]=common;
    s->shape.tail_samples=(unsigned)ceil(delay);
#endif
#if EXPERIMENTAL_TRAJECTORY_SMOOTHING
    if(!smoothing_init(&s->smooth,filter?smoothing_window:1,s->origin))return false;
#endif
    return true;
}
bool sm_sample(sm_state_t *s,double fraction,bool source_done,sm_output_t *out) {
    if(!isfinite(fraction) || fraction<0 || fraction>1)return false;
    if(!s->draining) {
        for(unsigned a=0;a<3;a++)s->raw[a]=s->origin[a]+s->delta[a]*fraction;
        if(source_done) {
            s->draining=true;s->remaining=0;
#if EXPERIMENTAL_INPUT_SHAPING
            s->remaining+=s->shape.tail_samples;
#endif
#if EXPERIMENTAL_TRAJECTORY_SMOOTHING
            s->remaining+=s->smooth.window-1;
#endif
        }
    } else {if(s->remaining)s->remaining--;s->drains++;}
    double p[3];memcpy(p,s->raw,sizeof(p));
#if EXPERIMENTAL_INPUT_SHAPING
    if(!shaper_sample(&s->shape,p,p))return false;
#endif
#if EXPERIMENTAL_TRAJECTORY_SMOOTHING
    if(!smoothing_sample(&s->smooth,p,p))return false;
#endif
    memset(out,0,sizeof(*out));int32_t next[3];
    for(unsigned a=0;a<3;a++) {
        if(!isfinite(p[a]) || p[a]<INT32_MIN || p[a]>INT32_MAX){s->faults++;return false;}
        next[a]=(int32_t)llround(p[a]);int64_t d=(int64_t)next[a]-s->last[a];
        out->steps[a]=(uint32_t)(d<0?-d:d);if(d<0)out->direction|=1u<<a;
        if(out->steps[a]>out->major)out->major=out->steps[a];
    }
    if(out->major>EXPERIMENTAL_FTM_MAX_STEP_HZ/EXPERIMENTAL_FTM_HZ){s->faults++;return false;}
    for(unsigned a=0;a<3;a++){s->velocity[a]=(p[a]-s->filtered[a])*EXPERIMENTAL_FTM_HZ;s->filtered[a]=p[a];}
    memcpy(s->last,next,sizeof(next));s->samples++;out->done=s->draining && s->remaining==0;return true;
}
