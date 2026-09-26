/* Regression tests use independent quadrature/kinematic bounds, not snapshots. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "experimental/scurve.h"
static unsigned count;
static void verify(double l,double v0,double v1,double vmax,double a,double j) {
 sc_profile_t p;assert(sc_plan(&p,l,v0,v1,vmax,a,j));count++;
 assert(p.count<=7);assert(p.duration>=l/vmax-1e-9);
 double x=0,v=v0,acc=0;
 for(unsigned n=0;n<p.count;n++) {
  const sc_phase_t *q=&p.phase[n];double dt=q->duration;
  assert(fabs(q->x-x)<1e-8*fmax(1,l));assert(fabs(q->v-v)<1e-8*fmax(1,vmax));assert(fabs(q->a-acc)<1e-8*fmax(1,a));
  assert(fabs(q->j)<=j*(1+1e-12));
  /* Simpson integrates the quadratic velocity exactly, independently of x(t). */
  double vm=q->v+q->a*dt/2+q->j*dt*dt/8,ve=q->v+q->a*dt+q->j*dt*dt/2;
  x+=dt*(q->v+4*vm+ve)/6;v=ve;acc=q->a+q->j*dt;
  assert(fabs(q->a)<=a*(1+1e-12));assert(fabs(acc)<=a*(1+1e-12));
  assert(vm>=-1e-9 && ve>=-1e-9 && vm<=vmax*(1+1e-10) && ve<=vmax*(1+1e-10));
 }
 assert(fabs(x-l)<1e-8*fmax(1,l));assert(fabs(v-v1)<1e-8*fmax(1,vmax));assert(fabs(acc)<1e-8*fmax(1,a));
 if(v0==0 && v1==0)assert(p.duration>=2*sqrt(l/a)*(1-1e-10));
 sc_sample_t last=sc_eval(&p,p.duration);assert(last.x==l && last.v==v1 && last.a==0);
 double previous=0;
 for(unsigned n=0;n<=1000;n++) {sc_sample_t s=sc_eval(&p,p.duration*n/1000);assert(s.x>=previous-1e-10);assert(s.x<=l+1e-9);assert(s.v<=vmax*(1+1e-10));assert(fabs(s.a)<=a*(1+1e-10));previous=s.x;}
}
int main(void) {
 const double lengths[]={1e-7,0.005,0.02,1,100,10000,1000000};
 const double accelerations[]={0.001,0.1,1,500,50000};
 const double feeds[]={1.0/60,1,50,500,10000};
 for(unsigned l=0;l<7;l++)for(unsigned a=0;a<5;a++)for(unsigned f=0;f<5;f++) {
  double length=lengths[l],acc=accelerations[a],speed=feeds[f],jerk=10*acc;
  verify(length,0,0,speed,acc,jerk);
  double v=sc_reachable(0,length/4,acc,jerk,speed)*0.99;
  verify(length,v,0,speed,acc,jerk);verify(length,0,v,speed,acc,jerk);verify(length,v,v,speed,acc,jerk);
  verify(length,v,v/3,speed,acc,jerk);verify(length,v/3,v,speed,acc,jerk);
 }
 sc_profile_t p;assert(!sc_plan(&p,0.001,100,0,100,500,5000));assert(!sc_plan(&p,1,0,0,100,0,1));assert(!sc_plan(&p,1,0,0,100,1,NAN));
 double diagonal=hypot(100,33),scale=diagonal/100;
 verify(diagonal,0,0,500*scale,500*scale,5000*scale);
 assert(sc_plan(&p,diagonal,0,0,500*scale,500*scale,5000*scale));
 printf("S-curve: %u analytically checked profiles; regression exact duration %.9f >= %.9f s\n",count,p.duration,2*sqrt(100.0/500));
}
