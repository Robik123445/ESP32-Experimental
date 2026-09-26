/* CPU/memory benchmark on the HOST. Not a target maximum step-frequency claim. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include "experimental/ftm.h"
#include "experimental/shaper.h"
#include "experimental/smoothing.h"
static ftm_t motion;
static shaper_t shape;
static smoothing_t smooth;
static double ns(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e9+t.tv_nsec;}
static void run(bool shaped) {
 const unsigned samples=500000;int32_t origin[3]={0};double zero[3]={0};
 ftm_config_t timing={20000,200};shaper_config_t config={.sample_hz=1000,.axis={{SHAPER_ZV,40,0.1},{SHAPER_ZV,63,0.1},{SHAPER_OFF,0,0}}};
 assert(ftm_init(&motion,&timing,origin));assert(shaper_init(&shape,&config,zero));assert(smoothing_init(&smooth,8,zero));
 unsigned tail=shaped?shape.tail_samples+7:0;int64_t position[3]={0};uint64_t total_ticks=0;
 double begin=ns(),batch_begin=begin,worst_batch=0;
 for(unsigned n=0;n<samples+tail;n++) {
  unsigned k=n<samples?n:samples-1;double p[3]={k*0.25,k*0.125,0};
  if(shaped){assert(shaper_sample(&shape,p,p));assert(smoothing_sample(&smooth,p,p));}
  assert(ftm_submit(&motion,p)==FTM_OK);assert(ftm_pump(&motion)==FTM_OK);
  if(!n)assert(ftm_start(&motion)==FTM_OK);
  while(ftm_fill(&motion)) {ftm_event_t e;assert(ftm_pop(&motion,&e)==FTM_OK);total_ticks+=e.ticks;for(unsigned a=0;a<3;a++)if(e.step_mask&(1u<<a))position[a]+=(e.direction_mask&(1u<<a))?-1:1;}
  if(n%1000==999){double t=ns(),batch=t-batch_begin;if(batch>worst_batch)worst_batch=batch;batch_begin=t;}
 }
 double elapsed=ns()-begin;assert(position[0]==llround((samples-1)*0.25));assert(position[1]==llround((samples-1)*0.125));
 assert(total_ticks==(uint64_t)(samples+tail)*20000);assert(!motion.underruns);
 assert(ftm_finish(&motion)==FTM_OK);ftm_event_t end;assert(ftm_pop(&motion,&end)==FTM_END);
 printf("%s,%u,%.3f,%.3f,%zu,%u,%u,%lld,%lld\n",shaped?"FTM_ZV_SMOOTHING":"FTM_BYPASS",samples+tail,elapsed/(samples+tail),worst_batch/1000,
  sizeof(motion)+(shaped?sizeof(shape)+sizeof(smooth):0),motion.high_water,motion.underruns,(long long)position[0],(long long)position[1]);
}
int main(void){puts("profile,samples,host_ns_per_sample,worst_host_batch_ns_per_sample,state_bytes,queue_high_water,underruns,end_x_steps,end_y_steps");run(false);run(true);}
