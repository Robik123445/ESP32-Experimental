/* Actual planner/preparer/ISR, including producer starvation and timer sums. */
#define main core_suite_main
#include "core_harness.c"
#undef main
#include "parking_test.h"
static probe_state_t clear_probe(void){return (probe_state_t){0};}
int main(void) {
 parking_resume();
 for(unsigned mode=0;mode<3;mode++) {
   init();add(1,.3f,0,300);
   if(mode==0)plan_get_current_block()->condition.jog_motion=1;
   if(mode==1){sys.flags.is_homing=1;sys.homing_axis_lock.bits=7;}
   if(mode==2){sys.probing_state=Probing_Active;hal.probe.get_state=clear_probe;}
   prepare();assert(!prep.sample_active);
   for(segment_t *s=(segment_t *)segment_buffer_tail;s!=segment_buffer_head;s=s->next)assert(!s->ftm_ticks);
   run();check("native_special_mode",1,.3f,0);
 }
 init();add(1,0,0,100);plan_get_current_block()->steps.value[0]=UINT32_MAX;
 prepare();assert(sys.rt_exec_alarm==Alarm_AbortCycle && !running && segment_buffer_head==segment_buffer_tail);

 init();add(100,37,11,12345);prepare();st_wake_up();
 uint64_t sample_ticks=0;unsigned samples=0;
 while(running){prepare();interrupt_once();if(st.exec_segment){assert(st.exec_segment->ftm_ticks);sample_ticks+=period;
   if(st.step_count==0){assert(sample_ticks==hal.f_step_timer/EXPERIMENTAL_FTM_HZ);sample_ticks=0;samples++;}}}
 assert(samples>100);assert(!sample_underruns);check("exact_timer_grid",100,37,11);
 init();add(1000,0,0,30000);prepare();st_wake_up();
 while(running)interrupt_once(); /* Deliberately never refill. */
 assert(sample_underruns==1 && sys.rt_exec_alarm==Alarm_AbortCycle && sys.step_control.end_motion);
 int64_t stopped=pulses[0];prepare();assert(pulses[0]==stopped && !running);
 st_reset();assert(!sample_pending && !sample_underruns);
 init();add(100,0,0,6000);prepare();st_wake_up();interrupt_once();
 st_update_plan_block_parameters(true);assert(!running && sys.rt_exec_alarm==Alarm_AbortCycle);
 st_reset();assert(!sample_pending && !sample_motion.draining);
 /* Trapezoid is used when FTM is enabled independently of S-curve. */
 for(unsigned k=1;k<100;k++){
   sc_profile_t p;double l=k*.003,a=k*.7,cap=k*.9;
   assert(sc_plan_trapezoid(&p,l,0,0,cap,a));assert(p.duration+1e-12>=2*sqrt(l/a));
   for(unsigned n=0;n<=1000;n++){sc_sample_t s=sc_eval(&p,p.duration*n/1000);assert(s.x>=-1e-10 && s.x<=l+1e-10);assert(s.v>=-1e-10 && s.v<=cap+1e-10);assert(fabs(s.a)<=a+1e-10);}
 }
 init();settings.steppers.pulse_microseconds=100;add(100,0,0,30000);prepare();st_wake_up();
 while(running){prepare();if(running)interrupt_once();}
 assert(sys.rt_exec_alarm==Alarm_AbortCycle && sample_motion.faults==1);
 free(block_buffer);puts("FTM exact timer grids, pulse totals, underrun alarm, fast abort, reset and trapezoids passed");
}
