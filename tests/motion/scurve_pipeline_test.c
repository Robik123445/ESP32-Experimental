#define main core_suite_main
#include "core_harness.c"
#undef main
static unsigned rng=78421;
static double random_unit(void){rng=1664525*rng+1013904223;return (rng>>8)/16777216.0;}
int main(void) {
 for(unsigned trial=0;trial<120;trial++) {
  init();double acceleration=pow(10,-1+5*random_unit()),jerk=pow(10,1+5*random_unit());
  for(unsigned a=0;a<3;a++){settings.axis[a].acceleration=acceleration*3600;settings.axis[a].jerk=jerk*216000;}
  float x=0;for(unsigned b=0;b<20;b++){x+=0.005f+(float)(random_unit()*5);add(x,0,0,pow(10,4.4*random_unit()));}
  run();for(unsigned a=0;a<3;a++){assert(absolute_pulses[a]==expected_pulses[a]);assert(pulses[a]==expected_position[a]);}
 }
 init();add(100,0,0,12000);prepare();st_wake_up();
 for(unsigned n=0;n<1000;n++){prepare();interrupt_once();}
 add(200,0,0,12000);add(210,0,0,1000);
 while(running){prepare();interrupt_once();}
 check("late_streaming",210,0,0);assert(!sys.rt_exec_alarm);
 init();add(100,0,0,12000);add(200,0,0,12000);prepare();st_wake_up();
 for(unsigned n=0;n<1000;n++){prepare();interrupt_once();}
 sys.override.feed_rate=25;plan_update_velocity_profile_parameters();plan_cycle_reinitialize();
 while(running){prepare();interrupt_once();}
 check("override_reduction",200,0,0);assert(!sys.rt_exec_alarm);
 free(block_buffer);puts("120 deterministic randomized planner/ISR paths, late streaming and override passed");
}
