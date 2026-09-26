/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "shaper.h"
#include <math.h>
#include <string.h>

bool shaper_init(shaper_t *s,const shaper_config_t *c,const double origin[FTM_AXES]) {
    if(!s || !c || !origin || !isfinite(c->sample_hz) || c->sample_hz<=0)return false;
    shaper_kernel_t kernel[FTM_AXES]={0};unsigned tail=0;
    for(unsigned a=0;a<FTM_AXES;a++) {
        if(!isfinite(origin[a]))return false;
        shaper_axis_config_t axis=c->axis[a];
        if(axis.type==SHAPER_OFF) {kernel[a].count=1;kernel[a].impulse[0]=(shaper_impulse_t){0,1};continue;}
        if(axis.type!=SHAPER_ZV || !isfinite(axis.frequency_hz) || axis.frequency_hz<=0 ||
           axis.frequency_hz>=c->sample_hz/2 || !isfinite(axis.damping) || axis.damping<0 || axis.damping>=1)
            return false;
        double root=sqrt(1-axis.damping*axis.damping);
        double delay=c->sample_hz/(2*axis.frequency_hz*root);
        if(!isfinite(delay) || delay>SHAPER_HISTORY-1)return false;
        double k=exp(-3.14159265358979323846*axis.damping/root),a1=k/(1+k);
        kernel[a].count=2;kernel[a].impulse[0]=(shaper_impulse_t){0,1-a1};kernel[a].impulse[1]=(shaper_impulse_t){delay,a1};
        unsigned n=(unsigned)ceil(delay);if(n>tail)tail=n;
    }
    memset(s,0,sizeof(*s));s->config=*c;memcpy(s->kernel,kernel,sizeof(kernel));s->tail_samples=tail;
    for(unsigned a=0;a<FTM_AXES;a++)for(unsigned n=0;n<SHAPER_HISTORY;n++)s->history[a][n]=origin[a];
    return true;
}
bool shaper_sample(shaper_t *s,const double input[FTM_AXES],double output[FTM_AXES]) {
    if(!s || !input || !output)return false;
    for(unsigned a=0;a<FTM_AXES;a++)if(!isfinite(input[a]))return false;
    for(unsigned a=0;a<FTM_AXES;a++)s->history[a][s->cursor]=input[a];
    for(unsigned a=0;a<FTM_AXES;a++) {
        double reference=s->history[a][s->cursor],sum=0;
        for(unsigned j=0;j<s->kernel[a].count;j++) {
            shaper_impulse_t p=s->kernel[a].impulse[j];unsigned n=(unsigned)p.delay;
            double fraction=p.delay-n;
            double newer=s->history[a][(s->cursor+SHAPER_HISTORY-n)%SHAPER_HISTORY];
            double older=s->history[a][(s->cursor+SHAPER_HISTORY-n-1)%SHAPER_HISTORY];
            sum+=p.weight*(newer+(older-newer)*fraction-reference);
        }
        output[a]=reference+sum;
    }
    s->cursor=(s->cursor+1)%SHAPER_HISTORY;return true;
}
