/* SPDX-License-Identifier: GPL-3.0-or-later
 * Application policy around the reusable ESP32-P4 HAL. No GPIO/timer driver
 * implementation lives here; physical pulses and enable writes belong to p4_driver.
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "p4_driver.h"
#include "feedback.h"
#include "grbl/protocol.h"
#include "grbl/state_machine.h"
#include "bridge.h"
#include "cycle.h"
#include "spindle.h"
#include "serial.h"
#include "storage.h"
#include "preferences.h"
#include "network.h"
#include "update.h"
#include "critical.h"
#include "diagnostics_internal.h"
#include "controller.h"
static portMUX_TYPE policy_lock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t disabled_requested, disabled_applied;
static bool axis_stop_requested, axis_change_pending;
static void irq_disable(void) { portENTER_CRITICAL(&policy_lock); }
static void irq_enable(void) { portEXIT_CRITICAL(&policy_lock); }
bool lathe_axis_change_pending(void)
{
    irq_disable();
    bool pending = axis_change_pending;
    irq_enable();
    return pending;
}
void lathe_axis_set_disabled(char axis, bool disabled)
{
    uint8_t bit = axis == 'X' ? X_AXIS_BIT : axis == 'Z' ? Z_AXIS_BIT : 0;
    irq_disable();
    uint8_t next = disabled ? disabled_requested | bit : disabled_requested & ~bit;
    if (next != disabled_requested) {
        disabled_requested = next;
        axis_stop_requested = axis_change_pending = true;
    }
    irq_enable();
    lathe_saved_disabled_set(next);
}
static void axis_enable_poll(void)
{
    irq_disable();
    bool stop = axis_stop_requested;
    axis_stop_requested = false;
    bool pending = axis_change_pending;
    irq_enable();
    if (stop) {
        // Native STOP decelerates and discards the planner and input queues.
        // Keep torque until the final pulse, including when leaving spindle sync.
        lathe_cycle_cancel();
        lathe_spindle_follow_braking();
        protocol_enqueue_realtime_command(CMD_STOP);
        return;
    }
    if (!pending || (sys.rt_exec_state & EXEC_STOP) || !p4_motion_idle() ||
        st_is_stepping() || plan_get_current_block() || lathe_cycle_busy() ||
        !(state_get() == STATE_IDLE || state_get() == STATE_ALARM)) return;
    irq_disable();
    if (!axis_stop_requested) {
        if (p4_set_disabled_axes(disabled_requested)) {
            disabled_applied = disabled_requested;
            axis_change_pending = false;
        }
    }
    irq_enable();
}
static bool motion_allowed(void)
{
    return lathe_ui_ready() && !lathe_update_active();
}
static void realtime(sys_state_t state)
{
    axis_enable_poll();
    lathe_serial_poll();
    lathe_spindle_poll();
    lathe_bridge_poll();
    lathe_cycle_poll();
    lathe_network_poll();
    lathe_storage_poll();
}
extern void lathe_tmc_init(void);
extern bool lathe_audio_ready(void);
extern void lathe_task_report(void);
extern void lathe_audio_tone(unsigned,unsigned);
extern status_code_t lathe_tmc_command(sys_state_t,char *);
// Runs only in the core's system-command dispatcher, never from the LVGL task.
// Recheck here: motion may have been queued after the touchscreen idle sample.
static status_code_t zero_work_axis(sys_state_t state, char axis)
{
    if (axis != 'X' && axis != 'Z') return Status_InvalidStatement;
    if (state != STATE_IDLE || !p4_motion_idle() || st_is_stepping() ||
        plan_get_current_block() || lathe_cycle_busy() || lathe_update_active() ||
        lathe_axis_change_pending() || !lathe_ui_ready() || sys.abort || sys.alarm)
        return Status_IdleError;
    char block[] = "G54G10L20P1X0";
    block[sizeof(block) - 3] = axis;
    return gc_execute_block(block);
}
static status_code_t command(sys_state_t state, char *line)
{
    if (!strncmp(line, "P4ZERO=", 7))
        return strlen(line) == 8 ? zero_work_axis(state, line[7]) : Status_InvalidStatement;
    if(!strcmp(line,"P4TASKS")) {lathe_task_report();return Status_OK;}
    if(!strcmp(line,"P4AUDIO")) {hal.stream.write(lathe_audio_ready()?"[P4AUDIO:READY]\r\n":"[P4AUDIO:UNAVAILABLE]\r\n");lathe_audio_tone(1200,70);return Status_OK;}
    status_code_t network_result=lathe_network_command(state,line);
    if(network_result!=Status_Unhandled)return network_result;
    status_code_t storage_result=lathe_storage_command(state,line);
    if(storage_result!=Status_Unhandled)return storage_result;
    status_code_t tmc_result=lathe_tmc_command(state,line);
    if(tmc_result!=Status_Unhandled)return tmc_result;
    // The internal Thread batch queues native blocks instead of parser moves.
    if (!strcmp(line, "P4THREADENTRY") && (disabled_applied & (X_AXIS_BIT | Z_AXIS_BIT)))
        return Status_SettingDisabled;
    status_code_t cycle_result = lathe_cycle_command(state, line);
    if (cycle_result != Status_Unhandled) return cycle_result;
    status_code_t spindle_result = lathe_spindle_command(state, line);
    if (spindle_result != Status_Unhandled) return spindle_result;
    if (strcmp(line, "P4UI") == 0) {
        lathe_status_t snapshot;
        lathe_bridge_snapshot(&snapshot);
        char text[160];
        snprintf(text, sizeof(text), "[P4UI:READY:%u|COMMAND:%lu|COMPLETED:%lu|STATUS:%d|GENERATION:%lu|UI_UPDATES:%lu|UPTIME:%lu]\r\n",
            lathe_ui_ready(), (unsigned long)snapshot.command_id, (unsigned long)snapshot.completed_id,
            snapshot.command_status, (unsigned long)snapshot.stream_generation,
            (unsigned long)lathe_ui_updates(), (unsigned long)hal.get_elapsed_ticks());
        hal.stream.write(text);
        return Status_OK;
    }
    if (strncmp(line, "P4UITEST=", 9) == 0) {
        if (!LATHE_BENCH_ONLY && (strlen(line) != 10 || !strchr("VBWUQ", line[9]))) return Status_InvalidStatement;
        if (strlen(line) != 10) return Status_InvalidStatement;
        return lathe_ui_test_action(line[9]) ? Status_OK : Status_InvalidStatement;
    }
    if (strcmp(line, "P4SCREEN") == 0) {
        if (state != STATE_IDLE || !p4_motion_idle()) return Status_IdleError;
        return lathe_ui_screenshot(hal.stream.write) ? Status_OK : Status_SelfTestFailed;
    }
    #if LATHE_BENCH_ONLY
    if (strcmp(line, "P4ENCODERTEST") == 0) {
        if (state != STATE_IDLE || !p4_motion_idle() || lathe_spindle_simulator_active()) return Status_IdleError;
        bool ok = p4_feedback_selftest();
        hal.stream.write(ok ? "[P4ENCODERTEST:PASS|COUNTS:32000,-32000,0]\r\n" : "[P4ENCODERTEST:FAIL]\r\n");
        return ok ? Status_OK : Status_SelfTestFailed;
    }
    #endif
    if (!strcmp(line, "P4CRITICAL")) {
        lathe_critical_report();
        return Status_OK;
    }
    return Status_Unhandled;
}
void lathe_driver_snapshot(lathe_diagnostics_t *s)
{
    p4_driver_diagnostics_t p;
    p4_driver_snapshot(&p);
    s->x_pulses = p.x_pulses;
    s->z_pulses = p.z_pulses;
    s->x_counted = p.x_counted;
    s->z_counted = p.z_counted;
    s->isr_us = p.isr_us;
    s->pulse_min = p.pulse_min;
    s->pulse_max = p.pulse_max;
    s->late = p.late;
    s->overlap = p.overlap;
    s->fault = p.fault;
    s->deadline_kind = p.deadline_kind;
    s->deadline_elapsed = p.deadline_elapsed;
    s->deadline_period = p.deadline_period;
    s->deadline_counter = p.deadline_counter;
    s->enable_x = p.enable_x;
    s->enable_z = p.enable_z;
    irq_disable();
    s->disabled_requested = disabled_requested; s->disabled_applied = disabled_applied;
    s->axis_change_pending = axis_change_pending;
    irq_enable();
}
static status_code_t validate(modal_groups_t *commands, parser_state_t *state, parser_block_t *block, spindle_t *spindle)
{
    if(lathe_update_active() || !lathe_ui_ready())return Status_IdleError;
    if (lathe_axis_change_pending()) return Status_IdleError;
    // Include full-circle arcs and G28/G30, which can move without X/Z words.
    if (((disabled_applied & X_AXIS_BIT) && (block->words.x || block->words.u)) ||
        ((disabled_applied & Z_AXIS_BIT) && (block->words.z || block->words.w)) ||
        (disabled_applied && ((commands->G1 && (block->modal.motion == MotionMode_CwArc ||
            block->modal.motion == MotionMode_CcwArc)) ||
            block->non_modal_command == NonModal_GoHome_0 || block->non_modal_command == NonModal_GoHome_1))) {
        hal.stream.write("[MSG:Axis disabled]\r\n");
        return Status_SettingDisabled;
    }
    // Core uses three-axis storage; an absent Y must not silently move virtually.
    if (block->words.y || block->words.v) {
        hal.stream.write("[MSG:P4 has no Y axis]\r\n");
        return Status_GcodeUnsupportedCommand;
    }
    if ((block->modal.motion == MotionMode_CwArc || block->modal.motion == MotionMode_CcwArc)
        && block->modal.plane_select != PlaneSelect_ZX) {
        hal.stream.write("[MSG:P4 arcs require G18]\r\n");
        return Status_GcodeUnsupportedCommand;
    }
    // G33 is path-distance/revolution; each participating axis is constrained.
    // G76 remains a separate, unvalidated CNC recipe.
    if (block->modal.motion == MotionMode_Threading) return Status_GcodeUnsupportedCommand;
    return Status_Unhandled;
}
static bool setup(settings_t *settings)
{
    lathe_tmc_init();
    lathe_storage_upgrade_motion();
    return true;
}
static bool initialize(void)
{
    disabled_requested = disabled_applied = lathe_saved_disabled_get();
    if (!p4_set_disabled_axes(disabled_applied)) return false;
    hal.board = "Waveshare ESP32-P4 lathe controller";
    hal.get_position = lathe_storage_restore_position;
    lathe_storage_hal();
    lathe_spindle_init();
    lathe_bridge_init();
    return lathe_serial_init();
}
void lathe_controller_configure(void)
{
    const p4_driver_hooks_t hooks = {
        .initialize = initialize,
        .setup = setup,
        .feedback_ready = lathe_spindle_ready,
        .motion_allowed = motion_allowed,
        .realtime = realtime,
        .busy_wait = lathe_spindle_near_index,
        .on_wake = lathe_critical_reset,
        .on_idle = lathe_spindle_idle,
        .on_block = lathe_spindle_block,
        .on_step = lathe_spindle_edge,
        .command = command,
        .validate = validate,
        .rx_overflows = lathe_serial_overflows,
        .storage_ready = lathe_storage_ready,
    };
    ESP_ERROR_CHECK(p4_driver_configure(&hooks) ? ESP_OK : ESP_ERR_INVALID_STATE);
}
