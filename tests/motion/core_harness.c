/* Exercise the actual local planner, segment preparer and Bresenham ISR.
 * Fake HAL never opens a device. Included C files expose internal queue state
 * for deterministic test scheduling without production test hooks. */
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include "../../main/grbl/planner.c"
#include "../../main/grbl/stepper.c"

grbl_hal_t hal;
grbl_t grbl;
system_t sys;
settings_t settings;
static spindle_ptrs_t spindle;
static bool running;
static int64_t pulses[3];
static uint64_t absolute_pulses[3], expected_pulses[3];
static int32_t expected_position[3];
static uint64_t ticks, calls;
static uint32_t period;
static double prep_ns, planner_ns, isr_ns, max_step_hz;
static unsigned segment_count, peak_fill;
static unsigned prepares;
static double motion_seconds;
static double now_ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec*1e9+t.tv_nsec; }
static sys_state_t fake_state=STATE_CYCLE;
sys_state_t state_get(void) { return fake_state; }
void report_plain(void *p) { (void)p; }
void gc_clear_output_commands(output_command_t *p) { assert(!p); }
void gc_output_message(char *p) { (void)p; }
bool task_add_immediate(foreground_task_ptr f, void *p) { (void)f;(void)p;return true; }
bool task_add_delayed(foreground_task_ptr f, void *p, uint32_t ms) { (void)f;(void)p;(void)ms;return true; }
bool task_run_on_startup(foreground_task_ptr f, void *p) { (void)f;(void)p;return true; }
void task_delete(foreground_task_ptr f, void *p) { (void)f;(void)p; }
float spindle_set_rpm(spindle_ptrs_t *s,float r,override_t o) { (void)s;(void)o;return r; }
static uint_fast16_t atomic_value(volatile uint_fast16_t *v,uint_fast16_t b) { uint_fast16_t old=*v;*v=b;return old; }
static void atomic_set(volatile uint_fast16_t *v,uint_fast16_t b) { *v|=b; }
static void enable(axes_signals_t s,bool h) { (void)s;(void)h; }
static void idle(bool clear) { (void)clear;running=false; }
static void wake(void) { running=true; }
static void set_period(uint32_t p) { assert(p);period=p; }
static void pulse(stepper_t *s) { for(unsigned a=0;a<3;a++) if(s->step_out.bits&(1u<<a)) {pulses[a]+=(s->dir_out.bits&(1u<<a))?-1:1;absolute_pulses[a]++;} }
#if EXPERIMENTAL_FTM
uint32_t experimental_step_min_period(void) { return (uint32_t)ceil((settings.steppers.pulse_microseconds+2)*hal.f_step_timer/1e6); }
#endif
static void init(void) {
 memset(&sys,0,sizeof(sys)); memset(pulses,0,sizeof(pulses));
 memset(absolute_pulses,0,sizeof(absolute_pulses));memset(expected_pulses,0,sizeof(expected_pulses));memset(expected_position,0,sizeof(expected_position));
 fake_state=STATE_CYCLE;ticks=calls=0;prepares=0;prep_ns=0;period=0;motion_seconds=0;planner_ns=isr_ns=max_step_hz=0;segment_count=peak_fill=0;
 sys.override.feed_rate=sys.override.rapid_rate=100;
 settings.planner_buffer_blocks=32;settings.junction_deviation=0.01f;
 settings.steppers.idle_lock_time=255;settings.steppers.pulse_microseconds=2;
 for(unsigned a=0;a<3;a++) { settings.axis[a].steps_per_mm=200;settings.axis[a].max_rate=30000;settings.axis[a].acceleration=500*3600;
#if ENABLE_JERK_ACCELERATION
 settings.axis[a].jerk=5000*216000.0f;
#endif
 }
 hal.set_value_atomic=atomic_value;hal.set_bits_atomic=atomic_set;hal.f_step_timer=20000000;hal.stepper.enable=enable;hal.stepper.go_idle=idle;
 hal.stepper.wake_up=wake;hal.stepper.cycles_per_tick=set_period;hal.stepper.pulse_start=pulse;
 assert(plan_reset());st_reset();
}
static void add(float x,float y,float z,float feed) {
 float target[]={x,y,z};plan_line_data_t data={0};data.feed_rate=feed;data.spindle.hal=&spindle;
 double begin=now_ns();assert(plan_buffer_line(target,&data));planner_ns+=now_ns()-begin;
 for(unsigned a=0;a<3;a++){int32_t target_steps=lroundf(target[a]*settings.axis[a].steps_per_mm);expected_pulses[a]+=llabs((int64_t)target_steps-expected_position[a]);expected_position[a]=target_steps;}
}
static void interrupt_once(void) { double begin=now_ns();stepper_driver_interrupt_handler();isr_ns+=now_ns()-begin;ticks+=period;assert(++calls<200000000); }
static void prepare(void) { segment_t *first=(segment_t *)segment_buffer_head;double t=now_ns();st_prep_buffer();prep_ns+=now_ns()-t;prepares++;
 for(segment_t *s=first;s!=segment_buffer_head;s=s->next){segment_count++;double hz=hal.f_step_timer/((double)s->cycles_per_tick*(1u<<s->amass_level));if(hz>max_step_hz)max_step_hz=hz;}
 unsigned fill=0;for(segment_t *s=(segment_t *)segment_buffer_tail;s!=segment_buffer_head;s=s->next)fill++;if(fill>peak_fill)peak_fill=fill;
 for(segment_t *s=(segment_t *)segment_buffer_tail;s!=segment_buffer_head;s=s->next) { assert(isfinite(s->current_rate));assert(s->current_rate>=0);assert(s->cycles_per_tick>0); }
}
static void run(void) {
 prepare();st_wake_up();
 while(running) { prepare();interrupt_once(); }
 assert(!sys.rt_exec_alarm);assert(!plan_get_current_block());
}
static void check(const char *name,float x,float y,float z) {
 float end[]={x,y,z};for(unsigned a=0;a<3;a++) { int32_t wanted=lroundf(end[a]*settings.axis[a].steps_per_mm);if(sys.position[a]!=wanted||pulses[a]!=wanted||absolute_pulses[a]!=expected_pulses[a]) { fprintf(stderr,"%s axis %u wanted %d position %ld pulses %lld\n",name,a,wanted,(long)sys.position[a],(long long)pulses[a]);abort(); } }
 motion_seconds=ticks/(double)hal.f_step_timer;
 printf("%s,%d,%.6f,%llu,%.0f,%u,%.0f,%.0f,%u,%u,%.3f\n",name,ENABLE_JERK_ACCELERATION,ticks/(double)hal.f_step_timer,(unsigned long long)calls,prep_ns,prepares,planner_ns,isr_ns,segment_count,peak_fill,max_step_hz);
}
static void hold_resume(void) {
 init();add(100,20,0,6000);prepare();st_wake_up();
 for(unsigned i=0;i<1000;i++) { prepare();interrupt_once();assert(running); }
 sys.step_control.execute_hold=1;st_update_plan_block_parameters(false);
 while(running) { prepare();interrupt_once(); }
 assert(sys.position[0]>0 && sys.position[0]<20000);assert(prep.current_speed==0);
 for(unsigned a=0;a<3;a++) assert(pulses[a]==sys.position[a]);
 plan_cycle_reinitialize();sys.step_control.flags=0;run();check("hold_resume",100,20,0);
}
int main(void) {
 setvbuf(stdout,NULL,_IOLBF,0);
 puts("case,jerk,simulated_seconds,isr_calls,host_prep_ns,prep_calls,host_planner_ns,host_isr_ns,segments,peak_fill,max_requested_step_hz");
 init();add(10,5,1,60);run();check("low_feed",10,5,1);
 init();add(100,33,0,30000);run();check("high_feed",100,33,0);
 /* Necessary physical bound for X travelling 100 mm from/to rest at 500 mm/s².
  * A smooth profile cannot beat even an ideal bang-bang acceleration profile. */
 const double acceleration_lower_bound=2*sqrt(100.0/500.0);
 if(motion_seconds<acceleration_lower_bound) {
   fprintf(stderr,"SAFETY GATE FAILED: jerk=%d high_feed time %.9f < acceleration lower bound %.9f seconds\n",ENABLE_JERK_ACCELERATION,motion_seconds,acceleration_lower_bound);
   if(getenv("ENFORCE_ACCELERATION_GATE"))return 2;
 }
 init();add(0.005f,0,0,1200);run();check("one_step",0.005f,0,0);
 init();add(0.01f,0.015f,0.005f,30000);run();check("short_diagonal",0.01f,0.015f,0.005f);
 init();add(10,10,0,6000);add(-10,-10,0,6000);add(0,0,0,6000);run();check("reversal",0,0,0);
 init();add(10000,0,0,30000);run();check("long_move",10000,0,0);
 init();add(1,0,0,0.1f);run();check("very_low_feed",1,0,0);
 init();add(10,0,0,3000);add(10,10,0,3000);add(0,10,0,3000);run();check("corners",0,10,0);
 hold_resume();
 /* Changing feed caps and collinear lookahead exercise nonzero entry/exit. */
 init();add(5,0,0,300);add(10,0,0,6000);add(15,0,0,120);add(20,0,0,12000);run();check("changing_entry_exit",20,0,0);
 init();for(unsigned n=1;n<=30;n++)add(n*0.005f,0,0,30000);run();check("tiny_junctions",0.15f,0,0);
 init();for(unsigned a=0;a<3;a++)settings.axis[a].acceleration=0.1f*3600;
 add(0.02f,0.005f,0,30000);run();check("low_acceleration",0.02f,0.005f,0);
 init();for(unsigned a=0;a<3;a++)settings.axis[a].acceleration=50000*3600;
 add(1,0.5f,0,30000);run();check("high_acceleration",1,0.5f,0);
#if ENABLE_JERK_ACCELERATION
 init();add(100,0,0,6000);prepare();assert(last_segment_accel>0);st_reset();
 #if EXPERIMENTAL_S_CURVE
 assert(last_segment_accel==0);
 #endif
#endif
 #ifndef CORE_SUITE_REUSE
 free(block_buffer);
 #endif
 return 0;
}
