/* SPDX-License-Identifier: GPL-3.0-or-later
 * SI units: mm, seconds. Analytically integrated constant-jerk phases.
 * Profiles have zero acceleration at entry/exit. Foreground only. */
#ifndef EXPERIMENTAL_SCURVE_H
#define EXPERIMENTAL_SCURVE_H
#include <stdbool.h>
#define SC_PHASES 7
typedef struct { double t,x,v,a,j,duration; } sc_phase_t;
typedef struct {
    sc_phase_t phase[SC_PHASES];
    unsigned count;
    double length,duration,entry,exit,peak;
} sc_profile_t;
typedef struct { double x,v,a,j; } sc_sample_t;
double sc_transition_time(double v0,double v1,double acceleration,double jerk);
double sc_transition_distance(double v0,double v1,double acceleration,double jerk);
double sc_reachable(double speed,double distance,double acceleration,double jerk,double ceiling);
bool sc_plan(sc_profile_t *p,double length,double entry,double exit,double ceiling,double acceleration,double jerk);
bool sc_plan_trapezoid(sc_profile_t *p,double length,double entry,double exit,double ceiling,double acceleration);
sc_sample_t sc_eval(const sc_profile_t *p,double time);
#endif
