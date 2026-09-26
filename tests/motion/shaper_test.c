#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "experimental/shaper.h"
static shaper_t shaper;
int main(void) {
 double origin[3]={100,-20,4},out[3];
 shaper_config_t config={.sample_hz=1000,.axis={{SHAPER_ZV,40,0},{SHAPER_ZV,63,0.1},{SHAPER_OFF,0,0}}};
 assert(shaper_init(&shaper,&config,origin));
 for(unsigned n=0;n<600;n++) {assert(shaper_sample(&shaper,origin,out));for(unsigned a=0;a<3;a++)assert(fabs(out[a]-origin[a])<1e-12);}
 double endpoint[3]={300,-10,-9};
 for(unsigned n=0;n<=shaper.tail_samples;n++)assert(shaper_sample(&shaper,endpoint,out));
 for(unsigned a=0;a<3;a++)assert(fabs(out[a]-endpoint[a])<1e-12);
 /* Impulse area/DC gain survives fractional delays. */
 double zero[3]={0},area[3]={0};assert(shaper_init(&shaper,&config,zero));
 for(unsigned n=0;n<100;n++) {double in[3]={!n,!n,!n};assert(shaper_sample(&shaper,in,out));for(unsigned a=0;a<3;a++)area[a]+=out[a];}
 for(unsigned a=0;a<3;a++)assert(fabs(area[a]-1)<1e-12);
 /* At undamped 40 Hz, residual gain is low despite sample interpolation. */
 double re=0,im=0;assert(shaper_init(&shaper,&config,zero));
 for(unsigned n=0;n<100;n++) {double in[3]={!n,0,0};assert(shaper_sample(&shaper,in,out));double phase=2*3.14159265358979323846*40*n/1000;re+=out[0]*cos(phase);im+=out[0]*sin(phase);}
 double residual=hypot(re,im);assert(residual<0.005);
 /* A diagonal under distinct filters is not necessarily a diagonal. */
 assert(shaper_init(&shaper,&config,zero));double error=0;
 for(unsigned n=0;n<100;n++){double in[3]={n,n,0};assert(shaper_sample(&shaper,in,out));double e=fabs(out[0]-out[1])/sqrt(2);if(e>error)error=e;}
 assert(error>0.1);
 config.axis[0].frequency_hz=0;assert(!shaper_init(&shaper,&config,zero));
 config.axis[0].frequency_hz=40;config.axis[0].damping=1;assert(!shaper_init(&shaper,&config,zero));
 config.axis[0].damping=0;config.axis[0].type=SHAPER_ZVD;assert(!shaper_init(&shaper,&config,zero));
 config.axis[0].type=SHAPER_ZV;config.axis[0].frequency_hz=0.1;assert(!shaper_init(&shaper,&config,zero));
 printf("ZV tests passed; state=%zu bytes; 40Hz residual=%.8f; unequal-filter diagonal error=%.6f steps\n",sizeof(shaper),residual,error);
}
