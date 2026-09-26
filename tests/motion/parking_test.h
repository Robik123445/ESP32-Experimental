/* Directly exercise the core's save/restore protocol used by parking. */
static void parking_resume(void) {
 init();settings.parking.flags.enabled=1;add(100,20,0,6000);prepare();st_wake_up();
 for(unsigned n=0;n<1000;n++){prepare();interrupt_once();}
 sys.step_control.execute_hold=1;st_update_plan_block_parameters(false);
 while(running){prepare();interrupt_once();}
 assert(prep.recalculate.hold_partial_block && !sys.rt_exec_alarm);
 float held[3];for(unsigned a=0;a<3;a++)held[a]=sys.position[a]/settings.axis[a].steps_per_mm;
 for(unsigned move=0;move<2;move++) {
   float target[3]={held[0],held[1],held[2]+(move?0:2)};
   plan_line_data_t data={0};data.feed_rate=300;data.condition.system_motion=1;data.spindle.hal=&spindle;
   assert(plan_buffer_line(target,&data));expected_pulses[2]+=400;
   sys.step_control.flags=0;sys.step_control.execute_sys_motion=1;st_parking_setup_buffer();prepare();
   assert(!prep.sample_active);st_wake_up();while(running){prepare();interrupt_once();}
   assert(!sys.rt_exec_alarm);sys.step_control.execute_sys_motion=0;st_parking_restore_buffer();
 }
 plan_cycle_reinitialize();sys.step_control.flags=0;run();check("parking_resume",100,20,0);
 settings.parking.flags.enabled=0;
}
