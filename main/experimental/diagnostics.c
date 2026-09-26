/* SPDX-License-Identifier: GPL-3.0-or-later
 * Foreground trace and idle-only no-output math benchmark.
 * Live FTM uses the native segment queue. This benchmark never reaches GPIO.
 */
#include "grbl/hal.h"
#include "grbl/nvs_buffer.h"
#include "grbl/state_machine.h"
#include "diagnostics.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#if EXPERIMENTAL_FTM
#include "ftm.h"
#include "sample_motion.h"
static ftm_t bench;
#endif
#if EXPERIMENTAL_INPUT_SHAPING
#include "shaper.h"
static shaper_t shaper;

#endif
#if EXPERIMENTAL_TRAJECTORY_SMOOTHING
#include "smoothing.h"
static smoothing_t smoothing;

#endif
#if EXPERIMENTAL_FTM && EXPERIMENTAL_MOTION_DIAGNOSTICS
static shaper_config_t config={.sample_hz=EXPERIMENTAL_FTM_HZ};
static unsigned window=1;
#endif

#if EXPERIMENTAL_FTM && EXPERIMENTAL_MOTION_DIAGNOSTICS
enum {
    EM_Setting_XType=780, EM_Setting_XFrequency, EM_Setting_XDamping,
    EM_Setting_YType, EM_Setting_YFrequency, EM_Setting_YDamping,
    EM_Setting_Smoothing
};
#define EM_SETTINGS_MAGIC 0x454d3031u
typedef struct { uint32_t magic; shaper_config_t config; uint8_t window; } em_settings_store_t;
static nvs_address_t em_settings_address;
static em_settings_store_t em_settings;
static bool em_loading;

static bool em_idle(void)
{
    return em_loading || (state_get() == STATE_IDLE && plan_get_current_block() == NULL && !st_is_stepping());
}

static bool em_apply(const shaper_config_t *candidate, unsigned smoothing)
{
    if(!em_idle() || !sm_configure(candidate, smoothing))
        return false;
    config = *candidate;
    window = smoothing;
    em_settings.magic = EM_SETTINGS_MAGIC;
    em_settings.config = config;
    em_settings.window = (uint8_t)window;
    return true;
}

static status_code_t em_set_float(setting_id_t id, float value)
{
    if(!em_idle()) return Status_IdleError;
    if(!isfinite(value) || value < 0.0f)
        return Status_BadNumberFormat;
    shaper_config_t candidate = config;
    switch(id) {
        case EM_Setting_XFrequency: candidate.axis[0].frequency_hz = value; break;
        case EM_Setting_XDamping: candidate.axis[0].damping = value; break;
        case EM_Setting_YFrequency: candidate.axis[1].frequency_hz = value; break;
        case EM_Setting_YDamping: candidate.axis[1].damping = value; break;
        default: return Status_SettingDisabled;
    }
    return em_apply(&candidate, window) ? Status_OK : Status_InvalidStatement;
}

static float em_get_float(setting_id_t id)
{
    switch(id) {
        case EM_Setting_XFrequency: return (float)config.axis[0].frequency_hz;
        case EM_Setting_XDamping: return (float)config.axis[0].damping;
        case EM_Setting_YFrequency: return (float)config.axis[1].frequency_hz;
        case EM_Setting_YDamping: return (float)config.axis[1].damping;
        default: return 0.0f;
    }
}

static status_code_t em_set_int(setting_id_t id, uint_fast16_t value)
{
    if(!em_idle()) return Status_IdleError;
    shaper_config_t candidate = config;
    unsigned next_window = window;
    switch(id) {
        case EM_Setting_XType:
        case EM_Setting_YType:
            if(value > SHAPER_ZV) return Status_InvalidStatement;
            candidate.axis[(int)id == EM_Setting_XType ? 0 : 1].type = (shaper_type_t)value;
            break;
        case EM_Setting_Smoothing:
            if(value < 1 || value > SMOOTHING_MAX_WINDOW) return Status_InvalidStatement;
            next_window = (unsigned)value;
            break;
        default: return Status_SettingDisabled;
    }
    return em_apply(&candidate, next_window) ? Status_OK : Status_InvalidStatement;
}

static uint32_t em_get_int(setting_id_t id)
{
    switch(id) {
        case EM_Setting_XType: return (uint32_t)config.axis[0].type;
        case EM_Setting_YType: return (uint32_t)config.axis[1].type;
        case EM_Setting_Smoothing: return window;
        default: return 0;
    }
}

static const setting_detail_t em_setting_details[] = {
    { (setting_id_t)EM_Setting_XType, Group_ExperimentalMotion, "X shaper type", NULL, Format_RadioButtons, "Off,ZV", NULL, NULL, Setting_NonCoreFn, em_set_int, em_get_int, NULL, {0} },
    { (setting_id_t)EM_Setting_XFrequency, Group_ExperimentalMotion, "X resonance frequency", "Hz", Format_Decimal, "##0.0", "0", "499", Setting_NonCoreFn, em_set_float, em_get_float, NULL, {0} },
    { (setting_id_t)EM_Setting_XDamping, Group_ExperimentalMotion, "X shaper damping", NULL, Format_Decimal, "#0.0000", "0", "0.9999", Setting_NonCoreFn, em_set_float, em_get_float, NULL, {0} },
    { (setting_id_t)EM_Setting_YType, Group_ExperimentalMotion, "Y shaper type", NULL, Format_RadioButtons, "Off,ZV", NULL, NULL, Setting_NonCoreFn, em_set_int, em_get_int, NULL, {0} },
    { (setting_id_t)EM_Setting_YFrequency, Group_ExperimentalMotion, "Y resonance frequency", "Hz", Format_Decimal, "##0.0", "0", "499", Setting_NonCoreFn, em_set_float, em_get_float, NULL, {0} },
    { (setting_id_t)EM_Setting_YDamping, Group_ExperimentalMotion, "Y shaper damping", NULL, Format_Decimal, "#0.0000", "0", "0.9999", Setting_NonCoreFn, em_set_float, em_get_float, NULL, {0} },
    { (setting_id_t)EM_Setting_Smoothing, Group_ExperimentalMotion, "Trajectory smoothing window", NULL, Format_Int8, "##0", "1", "64", Setting_NonCoreFn, em_set_int, em_get_int, NULL, {0} }
};

static const setting_group_detail_t em_setting_groups[] = {
    { .parent = Group_Root, .id = Group_ExperimentalMotion, .name = "Experimental motion", .is_available = NULL }
};

#ifndef NO_SETTINGS_DESCRIPTIONS
static const setting_descr_t em_settings_descriptions[] = {
    { (setting_id_t)EM_Setting_XType, "Enable the experimental X-axis ZV shaper; takes effect only when FTM and input shaping are compiled in." },
    { (setting_id_t)EM_Setting_XFrequency, "Measured X resonance frequency in hertz." },
    { (setting_id_t)EM_Setting_XDamping, "X-axis shaper damping ratio from 0 to less than 1." },
    { (setting_id_t)EM_Setting_YType, "Enable the experimental Y-axis ZV shaper; takes effect only when FTM and input shaping are compiled in." },
    { (setting_id_t)EM_Setting_YFrequency, "Measured Y resonance frequency in hertz." },
    { (setting_id_t)EM_Setting_YDamping, "Y-axis shaper damping ratio from 0 to less than 1." },
    { (setting_id_t)EM_Setting_Smoothing, "Common XYZ moving-average sample window; 1 disables smoothing." }
};
#endif

static void em_settings_save(void)
{
    if(em_settings_address)
        hal.nvs.memcpy_to_nvs(em_settings_address, (uint8_t *)&em_settings, sizeof(em_settings), true);
}

static void em_settings_restore(void)
{
    em_settings = (em_settings_store_t){ .magic = EM_SETTINGS_MAGIC, .config = { .sample_hz = EXPERIMENTAL_FTM_HZ }, .window = 1 };
    (void)em_apply(&em_settings.config, em_settings.window);
    em_settings_save();
}

static void em_settings_load(void)
{
    em_settings_store_t stored = {0};
    if(hal.nvs.memcpy_from_nvs((uint8_t *)&stored, em_settings_address, sizeof(stored), true) != NVS_TransferResult_OK ||
       stored.magic != EM_SETTINGS_MAGIC || !em_apply(&stored.config, stored.window)) {
        em_loading = true;
        em_settings_restore();
        em_loading = false;
    }
}

static void em_settings_init(void)
{
    static setting_details_t details = {
        .n_groups = sizeof(em_setting_groups) / sizeof(setting_group_detail_t),
        .groups = em_setting_groups,
        .settings = em_setting_details,
        .n_settings = sizeof(em_setting_details) / sizeof(setting_detail_t),
#ifndef NO_SETTINGS_DESCRIPTIONS
        .descriptions = em_settings_descriptions,
        .n_descriptions = sizeof(em_settings_descriptions) / sizeof(setting_descr_t),
#endif
        .save = em_settings_save,
        .load = em_settings_load,
        .restore = em_settings_restore
    };
    em_settings_address = nvs_alloc(sizeof(em_settings_store_t));
    if(em_settings_address)
        settings_register(&details);
}
#endif
#define TRACE_SIZE 64u
typedef struct { float velocity,acceleration,shaped[3]; uint32_t fill,steps; } trace_t;
static trace_t trace[TRACE_SIZE];
static uint32_t trace_head,trace_count,trace_overwritten;
static float previous_velocity;
void em_trace_reset(void) { trace_head=trace_count=trace_overwritten=0;previous_velocity=0; }
void em_trace_segment(float v,float dt,uint32_t fill,uint32_t steps) {
    trace[trace_head]=(trace_t){.velocity=v,.acceleration=dt>0?(v-previous_velocity)/(dt*3600):0,.fill=fill,.steps=steps};
    previous_velocity=v;trace_head=(trace_head+1)%TRACE_SIZE;
    if(trace_count<TRACE_SIZE)trace_count++;else trace_overwritten++;
}
void em_trace_shaped(const double velocity[3]) {
    unsigned n=(trace_head+TRACE_SIZE-1)%TRACE_SIZE;
    for(unsigned a=0;a<3;a++)trace[n].shaped[a]=(float)velocity[a];
}
static status_code_t report(sys_state_t state,char *args) {
    if(state!=STATE_IDLE)return Status_IdleError;
    if(args)return Status_InvalidStatement;
    char line[192];
    snprintf(line,sizeof(line),"[EM:ftm_output=%s,trace=%lu,overwritten=%lu,planner_free=%u]\r\n",
#if EXPERIMENTAL_FTM
        "NATIVE_SEGMENTS",
#else
        "OFF",
#endif
        (unsigned long)trace_count,(unsigned long)trace_overwritten,(unsigned)plan_get_block_buffer_available());hal.stream.write(line);
#if EXPERIMENTAL_FTM
    em_live_stats_t live;st_motion_diagnostics(&live);
    snprintf(line,sizeof(line),"[EM:live_fill=%lu,underruns=%lu,faults=%lu,last_block_samples=%lu,drains=%lu,state_bytes=%lu]\r\n",
        (unsigned long)live.fill,(unsigned long)live.underruns,(unsigned long)live.faults,
        (unsigned long)live.samples,(unsigned long)live.drains,(unsigned long)live.state_bytes);hal.stream.write(line);
    snprintf(line,sizeof(line),"[EM:prep_us=%llu,prep_max_us=%lu,prep_calls=%lu,shaped_steps_s=%.3f/%.3f/%.3f]\r\n",
        (unsigned long long)live.prep_us,(unsigned long)live.prep_max_us,(unsigned long)live.prep_calls,
        live.velocity[0],live.velocity[1],live.velocity[2]);hal.stream.write(line);
    snprintf(line,sizeof(line),"[EM:bench_only,fill=%lu,high_water=%lu,underruns=%lu,fault=%lu]\r\n",
        (unsigned long)ftm_fill(&bench),(unsigned long)bench.high_water,(unsigned long)bench.underruns,(unsigned long)bench.fault);hal.stream.write(line);
#endif
#if EXPERIMENTAL_INPUT_SHAPING
    snprintf(line,sizeof(line),"[EM:sample_hz=%.0f,X=%u/%.3f/%.3f,Y=%u/%.3f/%.3f]\r\n",config.sample_hz,
        (unsigned)config.axis[0].type,config.axis[0].frequency_hz,config.axis[0].damping,
        (unsigned)config.axis[1].type,config.axis[1].frequency_hz,config.axis[1].damping);hal.stream.write(line);
#endif
#if EXPERIMENTAL_TRAJECTORY_SMOOTHING
    snprintf(line,sizeof(line),"[EM:smoothing_window=%u]\r\n",window);hal.stream.write(line);
#endif
    return Status_OK;
}
static status_code_t dump(sys_state_t state,char *args) {
    if(state!=STATE_IDLE)return Status_IdleError;
    if(args)return Status_InvalidStatement;
    char line[192];unsigned n=(trace_head+TRACE_SIZE-trace_count)%TRACE_SIZE;
    for(unsigned i=0;i<trace_count;i++,n=(n+1)%TRACE_SIZE) {
        snprintf(line,sizeof(line),"[EMTRACE:v=%.5f,a_est=%.5f,fill=%lu,steps=%lu,shaped_steps_s=%.3f/%.3f/%.3f]\r\n",trace[n].velocity,trace[n].acceleration,
            (unsigned long)trace[n].fill,(unsigned long)trace[n].steps,trace[n].shaped[0],trace[n].shaped[1],trace[n].shaped[2]);hal.stream.write(line);
    }
    return Status_OK;
}
#if EXPERIMENTAL_INPUT_SHAPING || EXPERIMENTAL_TRAJECTORY_SMOOTHING
static status_code_t configure(sys_state_t state,char *args) {
    if(state!=STATE_IDLE || plan_get_current_block() || st_is_stepping())return Status_IdleError;
    if(!args)return Status_InvalidStatement;
#if EXPERIMENTAL_FTM
    if(!em_settings_address)return Status_SettingDisabled;
#endif
    double v[7];char *p=args,*end;
    for(unsigned i=0;i<7;i++) {
        v[i]=strtod(p,&end);
        if(p==end || !isfinite(v[i]) || (i<6?*end!=',':*end!='\0'))return Status_InvalidStatement;
        p=end+1;
    }
    if((v[0]!=0 && v[0]!=1) || (v[3]!=0 && v[3]!=1) || v[6]<1 || v[6]>SMOOTHING_MAX_WINDOW || floor(v[6])!=v[6])return Status_InvalidStatement;
    shaper_config_t candidate={.sample_hz=1000,.axis={{(shaper_type_t)v[0],v[1],v[2]},{(shaper_type_t)v[3],v[4],v[5]},{SHAPER_OFF,0,0}}};

    return em_apply(&candidate,(unsigned)v[6]) ? (em_settings_save(),Status_OK) : Status_InvalidStatement;
}
#endif
#if EXPERIMENTAL_FTM
static status_code_t benchmark(sys_state_t state,char *args) {
    if(state!=STATE_IDLE || plan_get_current_block() || st_is_stepping())return Status_IdleError;
    if(args)return Status_InvalidStatement;
    const int32_t origin[3]={0};const ftm_config_t timing={20000,200};
    if(!ftm_init(&bench,&timing,origin))return Status_InvalidStatement;
    unsigned tail=0;
#if EXPERIMENTAL_INPUT_SHAPING
    double zero[3]={0};if(!shaper_init(&shaper,&config,zero))return Status_InvalidStatement;
    tail+=shaper.tail_samples;
#endif
#if EXPERIMENTAL_TRAJECTORY_SMOOTHING
    double start[3]={0};if(!smoothing_init(&smoothing,window,start))return Status_InvalidStatement;
    tail+=window-1;
#endif
    int32_t counts[3]={0};double last[3]={0};uint64_t begin=hal.get_micros?hal.get_micros():0;
    double max_speed=0;
    for(unsigned n=0;n<256+tail;n++) {
        unsigned k=n<256?n:255;double p[3]={k*0.25,k*0.1,0};
#if EXPERIMENTAL_INPUT_SHAPING
        if(!shaper_sample(&shaper,p,p))return Status_InvalidStatement;
#endif
#if EXPERIMENTAL_TRAJECTORY_SMOOTHING
        if(!smoothing_sample(&smoothing,p,p))return Status_InvalidStatement;
#endif
        double speed=fabs(p[0]-last[0])*1000;if(speed>max_speed)max_speed=speed;
        memcpy(last,p,sizeof(last));
        if(ftm_submit(&bench,p)!=FTM_OK || ftm_pump(&bench)!=FTM_OK)return Status_InvalidStatement;
        if(n==0 && ftm_start(&bench)!=FTM_OK)return Status_InvalidStatement;
        while(ftm_fill(&bench)) {
            ftm_event_t e;if(ftm_pop(&bench,&e)!=FTM_OK)return Status_InvalidStatement;
            for(unsigned a=0;a<3;a++)if(e.step_mask&(1u<<a))counts[a]+=(e.direction_mask&(1u<<a))?-1:1;
        }
    }
    ftm_event_t end;
    if(ftm_finish(&bench)!=FTM_OK || ftm_pop(&bench,&end)!=FTM_END)return Status_InvalidStatement;
    uint64_t elapsed=hal.get_micros?hal.get_micros()-begin:0;
    char line[192];snprintf(line,sizeof(line),"[EMBENCH:NO_GPIO,samples=%u,us=%llu,XYZ=%ld/%ld/%ld,shaped_vx_max_steps_s=%.3f]\r\n",
        256+tail,(unsigned long long)elapsed,(long)counts[0],(long)counts[1],(long)counts[2],max_speed);hal.stream.write(line);
    return counts[0]==64 && counts[1]==26 && counts[2]==0 ? Status_OK : Status_InvalidStatement;
}
#endif
void my_plugin_init(void) {
    static const sys_command_t commands[]={
        {"EM",report,{.noargs=1},{.str="Experimental diagnostics (idle only)"}},
        {"EMTRACE",dump,{.noargs=1},{.str="Prepared segment trace (idle only)"}},
#if EXPERIMENTAL_FTM
        {"EMBENCH",benchmark,{.noargs=1},{.str="No-output synthetic FTM benchmark (idle only)"}},
#endif
#if EXPERIMENTAL_INPUT_SHAPING || EXPERIMENTAL_TRAJECTORY_SMOOTHING
        {"EMCONFIG",configure,{0},{.str="Live idle-only: Xtype,XHz,Xdamping,Ytype,YHz,Ydamping,window"}},
#endif
    };
    static sys_commands_t list={.n_commands=sizeof(commands)/sizeof(commands[0]),.commands=commands};
#if EXPERIMENTAL_FTM
    em_settings_init();
#endif
    system_register_commands(&list);
}
