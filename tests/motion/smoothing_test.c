#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "experimental/smoothing.h"
int main(void) {
 smoothing_t s;double zero[3]={0},out[3];
 assert(smoothing_init(&s,1,zero));
 double p[3]={1.234,-56,999};assert(smoothing_sample(&s,p,out));for(unsigned a=0;a<3;a++)assert(out[a]==p[a]);
 assert(smoothing_init(&s,16,zero));
 for(unsigned n=0;n<10000;n++) {double in[3]={n,2*n,0};assert(smoothing_sample(&s,in,out));assert(out[1]==2*out[0]);assert(out[0]<=in[0]);}
 for(unsigned n=0;n<16;n++)assert(smoothing_sample(&s,p,out));
 for(unsigned a=0;a<3;a++)assert(out[a]==p[a]);
 assert(!smoothing_init(&s,0,zero));assert(!smoothing_init(&s,65,zero));p[0]=NAN;assert(!smoothing_sample(&s,p,out));
 printf("Smoothing tests passed; state=%zu bytes\n",sizeof(s));
}
