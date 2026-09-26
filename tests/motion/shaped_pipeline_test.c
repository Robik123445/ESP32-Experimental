#define CORE_SUITE_REUSE
#define main core_suite_main
#include "core_harness.c"
#undef main
#include "parking_test.h"
int main(void) {
 shaper_config_t c={.sample_hz=EXPERIMENTAL_FTM_HZ,.axis={{SHAPER_ZV,40,.1},{SHAPER_ZV,63,.12},{SHAPER_OFF,0,0}}};
 assert(sm_configure(&c,1));
 assert(core_suite_main()==0); /* Original cases, exact endpoints and absolute pulses. */
 assert(sm_configure(&c,8));
 assert(core_suite_main()==0);
 parking_resume();
 /* Verify shared kernel cannot bend an XYZ line before step quantization. */
 static sm_state_t s;int32_t origin[3]={-150,270,6},delta[3]={2000,-700,500};
 assert(sm_begin(&s,origin,delta,true));assert(s.shape.kernel[0].count==4);
 double last[3]={-150,270,6},last_v[3]={0};sc_profile_t curve;
 assert(sc_plan(&curve,1,0,0,2,4,40));unsigned n=0;sm_output_t out;
 do {
   double t=++n/(double)EXPERIMENTAL_FTM_HZ;sc_sample_t p=sc_eval(&curve,t);
   assert(sm_sample(&s,p.x,t>=curve.duration,&out));
   /* Inspect continuous filtered positions before integer quantization. */
   double filtered[3];
   for(unsigned a=0;a<3;a++) {
      filtered[a]=s.filtered[a];
      double u=(filtered[a]-origin[a])/delta[a];
      double vx=(filtered[a]-last[a])*1000;
      assert(fabs(vx)<=fabs(delta[a])*2+1e-7);
      assert(fabs((vx-last_v[a])*1000)<=fabs(delta[a])*4+1e-5);
      if(a)assert(fabs(u-(filtered[0]-origin[0])/delta[0])<1e-12);
      assert(fabs(s.last[a]-filtered[a])<=.500000001);
      last[a]=filtered[a];last_v[a]=vx;
   }
 } while(!out.done);
 for(unsigned a=0;a<3;a++)assert(s.last[a]==origin[a]+delta[a]);
 /* Invalid compound delay must leave the previous valid settings unchanged. */
 shaper_config_t bad=c;bad.axis[0].frequency_hz=1.1;bad.axis[1].frequency_hz=1.1;
 assert(!sm_configure(&bad,8));shaper_config_t saved;unsigned w;sm_get_config(&saved,&w);
 assert(saved.axis[0].frequency_hz==40 && w==8);
 init();add(100,33,0,6000);prepare();st_wake_up();
 for(unsigned k=0;k<300;k++){prepare();interrupt_once();}
 add(-10,-4,0,3000);add(1,2,0,100);
 while(running){prepare();interrupt_once();}
 assert(!sys.rt_exec_alarm);check("shaped_late_streaming",1,2,0);
 init();add(100,0,0,6000);prepare();st_wake_up();
 for(unsigned k=0;k<300;k++){prepare();interrupt_once();}
 sys.override.feed_rate=25;plan_update_velocity_profile_parameters();plan_cycle_reinitialize();
 while(running){prepare();interrupt_once();}
 assert(!sys.rt_exec_alarm);check("shaped_override",100,0,0);
 free(block_buffer);
 puts("Live ZV and smoothing: original planner/STEP cases, common-path geometry, velocity/acceleration and endpoint passed");
}
