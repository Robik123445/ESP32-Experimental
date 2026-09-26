/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "smoothing.h"
#include <math.h>
#include <string.h>
bool smoothing_init(smoothing_t *s,unsigned window,const double origin[FTM_AXES]) {
    if(!s || !origin || !window || window>SMOOTHING_MAX_WINDOW)return false;
    for(unsigned a=0;a<FTM_AXES;a++)if(!isfinite(origin[a]))return false;
    memset(s,0,sizeof(*s));s->window=window;
    for(unsigned n=0;n<window;n++)memcpy(s->history[n],origin,sizeof(s->history[n]));
    return true;
}
bool smoothing_sample(smoothing_t *s,const double in[FTM_AXES],double out[FTM_AXES]) {
    if(!s || !in || !out || !s->window)return false;
    for(unsigned a=0;a<FTM_AXES;a++)if(!isfinite(in[a]))return false;
    memcpy(s->history[s->cursor],in,sizeof(s->history[0]));
    /* Recompute bounded window to avoid drift in a running floating-point sum.
     * Sum offsets from the newest point: a fully drained constant is exact. */
    for(unsigned a=0;a<FTM_AXES;a++) {
        double reference=s->history[s->cursor][a],sum=0;
        for(unsigned n=0;n<s->window;n++)sum+=s->history[n][a]-reference;
        out[a]=reference+sum/s->window;
    }
    s->cursor=(s->cursor+1)%s->window;return true;
}
