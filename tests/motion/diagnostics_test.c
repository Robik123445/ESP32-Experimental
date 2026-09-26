/* Host exercise of the exact firmware system-command implementation. */
#define main core_test_main
#include "core_harness.c"
#undef main
#include "../../main/experimental/diagnostics.c"
static sys_commands_t *registered;
static setting_details_t *registered_settings;
static uint8_t saved_nvs[256];static uint32_t saved_address,saved_size;static bool saved_valid;static em_settings_store_t saved_snapshot;
void settings_register(setting_details_t *details){registered_settings=details;}
nvs_address_t nvs_alloc(size_t size){assert(size<=sizeof(saved_nvs));saved_size=(uint32_t)size;saved_address=0x100;return saved_address;}
static bool nvs_write(uint32_t at,uint8_t *src,uint32_t size,bool checksum){assert(at==saved_address&&size==saved_size&&checksum);memcpy(saved_nvs,src,size);memcpy(&saved_snapshot,src,size);saved_valid=true;return true;}
static bool nvs_read(uint8_t *dest,uint32_t at,uint32_t size,bool checksum){assert(at==saved_address&&size==saved_size&&checksum);if(!saved_valid)return false;memcpy(dest,saved_nvs,size);return true;}
static const setting_detail_t *find_setting(setting_id_t id){for(unsigned n=0;n<registered_settings->n_settings;n++)if(registered_settings->settings[n].id==id)return &registered_settings->settings[n];return NULL;}
static void set_float(setting_id_t id,float v){const setting_detail_t *s=find_setting(id);assert(s);assert(((setting_set_float_ptr)s->value)(id,v)==Status_OK);registered_settings->save();}
static void set_int(setting_id_t id,uint_fast16_t v){const setting_detail_t *s=find_setting(id);assert(s);assert(((setting_set_int_ptr)s->value)(id,v)==Status_OK);registered_settings->save();}
void system_register_commands(sys_commands_t *commands) { registered=commands; }
static uint64_t micros(void){return (uint64_t)(now_ns()/1000);}
static void output(const char *text) { fputs(text,stdout); }
int main(void) {
 init();hal.stream.write=output;hal.get_micros=micros;hal.nvs.memcpy_to_nvs=nvs_write;hal.nvs.memcpy_from_nvs=nvs_read;
 my_plugin_init();assert(registered && registered->n_commands==4);
 assert(registered_settings && registered_settings->n_settings==7 && registered_settings->n_groups==1);
 assert(registered_settings->groups[0].parent==Group_Root && registered_settings->groups[0].id==Group_ExperimentalMotion);
 registered_settings->load(); /* Empty host NVS restores the safe bypass defaults. */
 sys_state_t prior=fake_state;fake_state=STATE_IDLE;
 set_float(EM_Setting_XFrequency,40);set_float(EM_Setting_XDamping,.1f);
 set_float(EM_Setting_YFrequency,63);set_float(EM_Setting_YDamping,.12f);
 set_int(EM_Setting_Smoothing,8);set_int(EM_Setting_XType,1);set_int(EM_Setting_YType,1);
 assert(saved_valid);
 registered_settings->load();shaper_config_t persistent;unsigned persisted_window;sm_get_config(&persistent,&persisted_window);
 assert(persistent.axis[0].type==SHAPER_ZV && persistent.axis[0].frequency_hz==40);
 assert(persistent.axis[1].frequency_hz==63 && persisted_window==8);
 const setting_detail_t *x_frequency=find_setting(EM_Setting_XFrequency);
 assert(((setting_set_float_ptr)x_frequency->value)(EM_Setting_XFrequency,.5f)==Status_InvalidStatement);
 assert(saved_snapshot.config.axis[0].frequency_hz==40);
 fake_state=STATE_CYCLE;
 assert(((setting_set_float_ptr)x_frequency->value)(EM_Setting_XFrequency,42.0f)==Status_IdleError);
 fake_state=STATE_IDLE;
 saved_nvs[0]=0;registered_settings->load();assert(saved_snapshot.magic==EM_SETTINGS_MAGIC);sm_get_config(&persistent,&persisted_window);
 assert(persistent.axis[0].type==SHAPER_OFF && persisted_window==1);
 fake_state=prior;
 assert(report(STATE_CYCLE,NULL)==Status_IdleError);
 char valid[]="1,40,0.1,1,63,0.1,8";
 fake_state=STATE_IDLE;assert(configure(STATE_IDLE,valid)==Status_OK);fake_state=STATE_CYCLE;
 add(1,.3f,0,100);run();check("command_configured_motion",1,.3f,0);
 assert(sample_motion.shape.kernel[0].count==4);
 em_live_stats_t stats;st_motion_diagnostics(&stats);assert(stats.prep_calls && stats.prep_us && stats.samples && stats.drains && !stats.underruns && !stats.fill);
 assert(benchmark(STATE_CYCLE,NULL)==Status_IdleError);
 assert(benchmark(STATE_IDLE,NULL)==Status_OK);
 assert(report(STATE_IDLE,NULL)==Status_OK);
 char invalid[]="1,0,0.1,1,63,0.1,8";assert(configure(STATE_IDLE,invalid)==Status_InvalidStatement);
 char badtype[]="2,40,0.1,1,63,0.1,8";assert(configure(STATE_IDLE,badtype)==Status_InvalidStatement);
 char trailing[]="1,40,0.1,1,63,0.1,8,x";assert(configure(STATE_IDLE,trailing)==Status_InvalidStatement);
 assert(config.axis[0].frequency_hz==40 && window==8 && config.axis[1].type==SHAPER_ZV);
 em_trace_reset(); /* Test ring overwrite independently of the motion trace above. */
 for(unsigned n=0;n<100;n++)em_trace_segment(n,0.01f/60,3,10);
 assert(trace_count==64 && trace_overwritten==36);
 em_trace_reset();assert(!trace_count && !trace_overwritten);
 free(block_buffer);puts("Diagnostics tests passed; no physical HAL output");
}
