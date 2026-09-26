/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "scurve.h"
#include <math.h>
#include <string.h>
static bool positive(double x) { return isfinite(x) && x>0; }
double sc_transition_time(double v0,double v1,double a,double j) {
    double dv=fabs(v1-v0),tj=fmin(a/j,sqrt(dv/j));
    return dv==0 ? 0 : tj+dv/(j*tj);
}
double sc_transition_distance(double v0,double v1,double a,double j) {
    return 0.5*(v0+v1)*sc_transition_time(v0,v1,a,j);
}
double sc_reachable(double v,double length,double a,double j,double cap) {
    if(!isfinite(v) || v<0 || !isfinite(length) || length<0 || !positive(a) || !positive(j) || !isfinite(cap) || cap<0)return NAN;
    if(cap<=v)return cap;
    double lo=v,hi=fmin(cap,sqrt(v*v+2*a*length));
    for(unsigned n=0;n<56;n++) {double mid=(lo+hi)*0.5;if(sc_transition_distance(v,mid,a,j)<=length)lo=mid;else hi=mid;}
    return lo;
}
static void append(sc_profile_t *p,sc_sample_t *s,double duration,double jerk) {
    if(duration<=0)return;
    sc_phase_t *q=&p->phase[p->count++];
    *q=(sc_phase_t){p->duration,s->x,s->v,s->a,jerk,duration};
    s->x+=s->v*duration+0.5*s->a*duration*duration+jerk*duration*duration*duration/6;
    s->v+=s->a*duration+0.5*jerk*duration*duration;s->a+=jerk*duration;p->duration+=duration;
}
static void transition(sc_profile_t *p,sc_sample_t *s,double target,double a,double j) {
    double dv=fabs(target-s->v);if(dv==0)return;
    double sign=target>s->v?1:-1,tj=fmin(a/j,sqrt(dv/j)),ta=fmax(0,dv/(j*tj)-tj);
    append(p,s,tj,sign*j);append(p,s,ta,0);append(p,s,tj,-sign*j);
    /* Only remove accumulated arithmetic roundoff, not a profile mismatch. */
    s->v=target;s->a=0;
}
bool sc_plan(sc_profile_t *p,double length,double entry,double exit,double cap,double a,double j) {
    if(!p || !positive(length) || !positive(cap) || !positive(a) || !positive(j) || !isfinite(entry) || !isfinite(exit) || entry<0 || exit<0 || entry>cap || exit>cap)return false;
    double required=sc_transition_distance(entry,exit,a,j);
    if(required>length+1e-10*fmax(1,length))return false;
    double lo=fmax(entry,exit),hi=cap;
    for(unsigned n=0;n<56;n++) {
        double mid=(lo+hi)*0.5;
        double d=sc_transition_distance(entry,mid,a,j)+sc_transition_distance(mid,exit,a,j);
        if(d<=length)lo=mid;else hi=mid;
    }
    double peak=lo,d=sc_transition_distance(entry,peak,a,j)+sc_transition_distance(peak,exit,a,j);
    memset(p,0,sizeof(*p));p->length=length;p->entry=entry;p->exit=exit;p->peak=peak;
    sc_sample_t s={.v=entry};transition(p,&s,peak,a,j);
    if(length>d)append(p,&s,(length-d)/peak,0);
    transition(p,&s,exit,a,j);
    return isfinite(p->duration) && p->duration>0 && p->count<=SC_PHASES;
}
sc_sample_t sc_eval(const sc_profile_t *p,double t) {
    if(t<=0)return (sc_sample_t){.v=p->entry};
    if(t>=p->duration)return (sc_sample_t){.x=p->length,.v=p->exit};
    const sc_phase_t *q=&p->phase[0];
    for(unsigned n=1;n<p->count && t>=p->phase[n].t;n++)q=&p->phase[n];
    double dt=t-q->t;
    return (sc_sample_t){q->x+q->v*dt+0.5*q->a*dt*dt+q->j*dt*dt*dt/6,q->v+q->a*dt+0.5*q->j*dt*dt,q->a+q->j*dt,q->j};
}

bool sc_plan_trapezoid(sc_profile_t *p,double l,double v0,double v1,double cap,double a) {
    if(!p || !positive(l) || !positive(cap) || !positive(a) || !isfinite(v0) || !isfinite(v1) || v0<0 || v1<0 || v0>cap || v1>cap || fabs(v1*v1-v0*v0)>2*a*l*(1+1e-6))return false;
    double peak=fmin(cap,sqrt(a*l+(v0*v0+v1*v1)/2));
    double distance=(2*peak*peak-v0*v0-v1*v1)/(2*a);
    memset(p,0,sizeof(*p));p->length=l;p->entry=v0;p->exit=v1;p->peak=peak;
    sc_sample_t state={.v=v0,.a=a};append(p,&state,(peak-v0)/a,0);
    state.a=0;state.v=peak;append(p,&state,fmax(0,l-distance)/peak,0);
    state.a=-a;append(p,&state,(peak-v1)/a,0);
    return p->duration>0 && isfinite(p->duration);
}
