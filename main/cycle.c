/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "cycle.h"
#include "thread_entry.h"
#include "follow.h"
#include "freertos/FreeRTOS.h"
#include "critical.h"
#include "grbl/planner.h"
#include "grbl/protocol.h"
#include "grbl/state_machine.h"
#include "grbl/stepper.h"
#include "serial.h"
#include "spindle.h"
#include "update.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static lathe_cycle_config_t request;
static lathe_cycle_status_t published;
static bool pending, stopping, owns_stream, advance_requested;
static lathe_cycle_plan_t plan;
static unsigned stage, pass, start, segment;
static uint32_t command_id;
static bool waiting_ack, cancel_requested, trace_enabled;
static bool pausing, internal_reset, recovering, reverse_cut;
static int cut_spindle_direction;
static bool thread_entry, restart_entry;
static char stop_reason[96] = "Cycle cancelled";
static const char *const names[] = {
    "Setup",   "Retract", "Approach", "Take up",         "Infeed",        "Register",      "Spindle", "Cut",
    "Retract", "Return",  "Take up",  "Return to start", "Finish infeed", "Restore phase", "Finish"};

bool lathe_cycle_busy(void)
{
    lathe_critical_enter(&lock, 2000 + __LINE__);
    bool busy = published.active;
    lathe_critical_exit(&lock);
    return busy || lathe_follow_busy();
}
bool lathe_cycle_owns_stream(void)
{
    lathe_critical_enter(&lock, 2000 + __LINE__);
    bool owns = owns_stream;
    lathe_critical_exit(&lock);
    return owns || lathe_follow_busy();
}
void lathe_cycle_snapshot(lathe_cycle_status_t *s)
{
    if (lathe_follow_selected()) {
        lathe_follow_snapshot(s);
        return;
    }
    lathe_critical_enter(&lock, 2000 + __LINE__);
    *s = published;
    lathe_critical_exit(&lock);
}
bool lathe_cycle_request(const lathe_cycle_config_t *c)
{
    if (lathe_follow_busy() || lathe_update_active() || lathe_axis_change_pending())
        return false;
    if (!lathe_operation_claim(LATHE_OWNER_PROFILE))
        return false;
    lathe_follow_clear();
    lathe_critical_enter(&lock, 2000 + __LINE__);
    bool ok = !published.active;
    if (ok) {
        request = *c;
        memcpy(stop_reason, "Cycle cancelled", sizeof("Cycle cancelled"));
        pending = true;
        stopping = advance_requested = false;
        published.active = true;
        published.pass = published.start = 0;
        memcpy(published.message, "Preparing cycle", sizeof("Preparing cycle"));
    }
    lathe_critical_exit(&lock);
    if (!ok)
        lathe_operation_release(LATHE_OWNER_PROFILE);
    return ok;
}
bool lathe_cycle_advance(void)
{
    lathe_critical_enter(&lock, 2000 + __LINE__);
    bool ok = published.active;
    if (ok)
        advance_requested = true;
    lathe_critical_exit(&lock);
    return ok;
}
void lathe_cycle_cancel(void)
{
    if (lathe_follow_busy()) {
        lathe_follow_cancel();
        return;
    }
    lathe_critical_enter(&lock, 2000 + __LINE__);
    if (published.active)
        stopping = true;
    lathe_critical_exit(&lock);
}
static void message(const char *text, bool active)
{
    char next[sizeof(published.message)] = {0};
    snprintf(next, sizeof(next), "%s", text);
    lathe_critical_enter(&lock, 2000 + __LINE__);
    published.active = active;
    if (!active)
        owns_stream = false;
    published.pass = pass < plan.config.passes ? pass + 1 : plan.config.passes;
    published.start = start + 1;
    memcpy(published.message, next, sizeof(next));
    lathe_critical_exit(&lock);
    if (!active) {
        lathe_operation_release(LATHE_OWNER_PROFILE);
        lathe_spindle_profile(false);
    }
    char line[180];
    snprintf(line, sizeof(line), "[P4CYCLE:PASS:%u|START:%u|STAGE:%s]\r\n", pass + 1, start + 1, text);
    hal.stream.write(line);
}
void lathe_cycle_reset(void)
{
    lathe_follow_reset();
    if (internal_reset && published.active && !stopping && !sys.alarm) {
        // A spindle stop/reversal interrupted only the cutting block. Keep
        // the operation, depth and start; never replay the plunge/retract.
        internal_reset = pausing = cancel_requested = waiting_ack = false;
        recovering = true;
        stage = 0;
        lathe_spindle_profile(true);
        // This runs inside stream flush, before parser/planner reset. Do not
        // write UART here: a blocked write can reenter realtime and enqueue a
        // new command into the stream that is still being discarded.
        lathe_critical_enter(&lock, 2000 + __LINE__);
        memcpy(published.message, "Armed; waiting for spindle", sizeof("Armed; waiting for spindle"));
        lathe_critical_exit(&lock);
        return;
    }
    internal_reset = pausing = recovering = thread_entry = restart_entry = false;
    lathe_spindle_profile(false);
    lathe_operation_release(LATHE_OWNER_PROFILE);
    lathe_critical_enter(&lock, 2000 + __LINE__);
    if (published.active)
        memcpy(published.message, stop_reason, sizeof(published.message));
    published.active = pending = stopping = owns_stream = false;
    lathe_critical_exit(&lock);
    waiting_ack = cancel_requested = false;
    command_id = 0;
}
static bool idle(void)
{
    return state_get() == STATE_IDLE && !st_is_stepping() && !plan_get_current_block();
}
static double axis_position(char axis)
{
    unsigned a = axis == 'X' ? X_AXIS : Z_AXIS;
    return (double)sys.position[a] / settings.axis[a].steps_per_mm;
}
static void wait_message(const char *text)
{
    if (strcmp(published.message, text)) message(text, true);
}
static bool prepare_cut(void)
{
    double rpm = lathe_spindle_profile_rpm();
    if (rpm == 0) {
        wait_message("Armed; waiting for spindle");
        return false;
    }
    cut_spindle_direction = rpm < 0 ? -1 : 1;
    reverse_cut = cut_spindle_direction != plan.spindle_direction;
    double approach = plan.approach, unused, feed;
    if (plan.config.operation == LATHE_ELLIPSE) {
        lathe_cycle_point(&plan, pass, 0, &unused, &approach, &feed);
        segment = lathe_cycle_segment_at(&plan, pass, axis_position('X'), axis_position('Z'));
    }
    double here = axis_position(plan.cut_axis);
    unsigned a = plan.cut_axis == 'X' ? X_AXIS : Z_AXIS;
    bool at_start = fabs(here - approach) < .5 / settings.axis[a].steps_per_mm;
    if (plan.config.operation == LATHE_ELLIPSE)
        at_start = at_start && fabs(axis_position('X') - unused) < .5 / settings.axis[X_AXIS].steps_per_mm;
    if (reverse_cut && at_start) {
        wait_message("At pass start; waiting for forward spindle rotation");
        return false;
    }
    // Native G33 cannot honor a lead above the actual axis maximum. Wait
    // armed rather than rejecting the operation; there is no preview RPM cap.
    if (plan.indexed && plan.lead * fabs(rpm) > settings.axis[a].max_rate) {
        wait_message("Armed; thread feed exceeds axis maximum");
        return false;
    }
    return true;
}
static void emit(void)
{
    if (!lathe_cycle_busy() || sys.abort)
        return;
    char line[116];
    bool cut = plan.config.operation == LATHE_CUT, ellipse = plan.config.operation == LATHE_ELLIPSE;
    double approach = plan.approach, infeed = lathe_cycle_depth(&plan, pass), endpoint = plan.finish;
    if (cut)
        endpoint = plan.cut_start + (plan.cut_end - plan.cut_start) * (pass + 1.0) / plan.config.passes;
    if (ellipse) {
        double unused;
        lathe_cycle_point(&plan, pass, 0, &infeed, &approach, &unused);
    }
    if (stage == 4 && plan.indexed) {
        // Keep X clear through acquisition. Infeed and cut are queued together
        // at stage 7, after the phase and spindle direction have been selected.
        lathe_spindle_entry_prepare();
        thread_entry = true;
        stage = 5;
    }
    if (stage == 5 && !prepare_cut()) return;
    if (stage == 7 && lathe_spindle_profile_rpm() * cut_spindle_direction <= 0) {
        // Spindle changed between the phase/M3 commands and cut submission.
        stage = 5;
        return;
    }
    switch (stage) {
    case 0:
        snprintf(line, sizeof(line), "G21G18G8G90G94");
        break;
    case 1:
    case 8:
        snprintf(line, sizeof(line), "G90G94G53G0%c%.6f", plan.depth_axis, plan.clearance);
        break;
    case 2:
    case 9:
        snprintf(line, sizeof(line), "G90G94G53G0%c%.6f", plan.cut_axis,
                 plan.indexed ? plan.takeup : approach);
        break;
    case 3:
    case 10:
        snprintf(line, sizeof(line), "G90G94G53G0%c%.6f", plan.cut_axis, approach);
        break;
    case 4:
        snprintf(line, sizeof(line), "G90G94G53G0%c%.6f", plan.depth_axis, infeed);
        break;
    case 5:
        snprintf(line, sizeof(line), "$P4PHASE=%u", plan.indexed ?
                 lathe_cycle_phase_at(&plan, start, axis_position(plan.cut_axis), cut_spindle_direction) : 0);
        break;
    case 6:
        snprintf(line, sizeof(line), "M%dS%.3f", cut_spindle_direction > 0 ? 3 : 4, fabs(lathe_spindle_profile_rpm()));
        break;
    case 7:
        if (ellipse) {
            double x, z, feed, unused;
            lathe_cycle_point(&plan, pass, segment + 1, &x, &z, &feed);
            if (reverse_cut) lathe_cycle_point(&plan, pass, segment, &x, &z, &unused);
            snprintf(line, sizeof(line), "G90G95G53G1X%.6fZ%.6fF%.6f", x, z, feed);
        } else if (plan.indexed && thread_entry)
            snprintf(line, sizeof(line), "$P4THREADENTRY");
        else if (plan.indexed)
            snprintf(line, sizeof(line), "G91G33%c%.6fK%.6f", plan.cut_axis,
                     (reverse_cut ? approach : endpoint) - axis_position(plan.cut_axis), plan.lead);
        else
            snprintf(line, sizeof(line), "G90G95G53G1%c%.6fF%.6f", plan.cut_axis, reverse_cut ? approach : endpoint,
                     plan.lead);
        break;
    case 11:
        snprintf(line, sizeof(line), "G90G94G53G0%c%.6f", plan.cut_axis, plan.cut_start);
        break;
    case 12:
        snprintf(line, sizeof(line), "G90G94G53G0%c%.6f", plan.depth_axis, plan.depth_start);
        break;
    case 13:
        snprintf(line, sizeof(line), "$P4PHASE=0");
        break;
    default:
        snprintf(line, sizeof(line), "G90G94M5");
        break;
    }
    command_id = lathe_bridge_cycle_submit(line);
    if (!command_id) {
        message("Cycle command queue failed", true);
        lathe_cycle_cancel();
        return;
    }
    waiting_ack = true;
    message(names[stage], true);
}
static void poll_cycle(void)
{
    lathe_critical_enter(&lock, 2000 + __LINE__);
    bool active = published.active, stop = stopping, begin = pending;
    lathe_cycle_config_t config = request;
    if (begin)
        pending = false;
    lathe_critical_exit(&lock);
    if (!active)
        return;
    if (begin) {
        pass = start = stage = segment = 0;
        waiting_ack = cancel_requested = false;
        pausing = internal_reset = recovering = reverse_cut = thread_entry = restart_entry = false;
        char error[96];
        lathe_cycle_machine_t machine = {.x = (double)sys.position[0] / settings.axis[0].steps_per_mm,
                                      .z = (double)sys.position[2] / settings.axis[2].steps_per_mm,
                                      .rpm = lathe_spindle_rpm(),
                                      .z_acceleration = settings.axis[2].acceleration / 3600.0,
                                      .z_max_rate = settings.axis[2].max_rate,
                                      .z_steps_mm = settings.axis[2].steps_per_mm,
                                      .x_acceleration = settings.axis[0].acceleration / 3600.0,
                                      .x_max_rate = settings.axis[0].max_rate,
                                      .x_steps_mm = settings.axis[0].steps_per_mm};
        if (!idle() || !lathe_bridge_empty() || lathe_serial_pending()) {
            message("Stop other motion before starting cycle", false);
            return;
        }
        if (gc_state.modal.scaling_active
#ifdef ROTATION_ENABLE
            || gc_state.modal.g5x_offset.data.rotation != 0
#endif
        ) {
            message("Cancel coordinate scaling/rotation before a cycle", false);
            return;
        }
        if (!lathe_cycle_plan(&config, &machine, &plan, error, sizeof(error))) {
            message(error, false);
            return;
        }
        lathe_spindle_profile(true);
        lathe_critical_enter(&lock, 2000 + __LINE__);
        owns_stream = true;
        lathe_critical_exit(&lock);
        char info[480];
        snprintf(info, sizeof(info),
                 "[P4PLAN:LEAD:%.6f|APPROACH:%.6f|FINISH:%.6f|CLEARANCE:%.6f|RPM:%.3f|CUT_LENGTH:%.6f|ACCEL:%.3f]\r\n",
                 plan.lead, plan.approach, plan.finish, plan.clearance,
                 machine.rpm, fabs(plan.finish-plan.approach), plan.cut_acceleration);
        hal.stream.write(info);
    }
    if (sys.alarm) {
        message("Cycle stopped by controller fault", false);
        return;
    }
    if (sys.abort) {
        if (internal_reset && !stop) return; // Retain the pass until the core finishes resetting.
        message(stop ? stop_reason : "Cycle reset", false);
        return;
    }
    if (!stop && state_get() == STATE_HOLD) {
        snprintf(stop_reason, sizeof(stop_reason), "Cycle cancelled by feed hold");
        message(stop_reason, true);
        lathe_cycle_cancel();
        stop = true;
    }
    double rpm = lathe_spindle_profile_rpm();
    if (thread_entry && stage == 7 && waiting_ack && lathe_spindle_entry_cutting())
        thread_entry = false;
    if (!stop && stage == 7 && waiting_ack &&
        (rpm * cut_spindle_direction < 0 || (rpm == 0 && (thread_entry || !lathe_spindle_waiting_index())))) {
        lathe_status_t completed;
        lathe_bridge_snapshot(&completed);
        // A cut that has already reached its endpoint must still retract,
        // even if the spindle stops in the same foreground iteration.
        if (!idle() || completed.completed_id != command_id || completed.command_status)
            pausing = true;
    }
    if (stop || pausing) {
        if (!cancel_requested) {
            restart_entry = thread_entry;
            lathe_spindle_follow_braking();
            lathe_bridge_discard_cycle_commands();
            // G33 intentionally disables feed hold. Motion cancel still uses
            // the core's normal deceleration, then we discard the stopped pass.
            system_set_exec_state_flag(EXEC_MOTION_CANCEL);
            cancel_requested = true;
            message(stop ? "Stopping cycle" : "Pausing cut; operation remains armed", true);
        }
        // The core's index-wait loop explicitly handles EXEC_STOP by resetting
        // before st_wake_up: no STEP output has started in that case.
        if (lathe_spindle_waiting_index()) {
            internal_reset = !stop;
            system_set_exec_state_flag(EXEC_STOP);
        }
        // Timer shutdown precedes the core's cycle-complete handling. Wait for
        // both, otherwise mc_reset correctly reports an in-motion reset alarm.
        else if (state_get() == STATE_IDLE && !st_is_stepping() && !sys.step_control.execute_hold) {
            internal_reset = !stop;
            protocol_enqueue_realtime_command(CMD_RESET);
        }
        return;
    }
    if (waiting_ack) {
        lathe_status_t status;
        lathe_bridge_snapshot(&status);
        if (status.completed_id != command_id)
            return;
        if (status.command_status) {
            if (stage == 7 && (status.command_status == Status_GcodeSpindleNotRunning ||
                              status.command_status == Status_GcodeMaxFeedRateExceeded)) {
                pausing = true; // RPM changed during submission; keep the pass.
                return;
            }
            snprintf(stop_reason, sizeof(stop_reason), "Cycle command rejected (%d)", status.command_status);
            message(stop_reason, true);
            lathe_cycle_cancel();
            return;
        }
        if (stage == 7 && plan.config.operation == LATHE_ELLIPSE &&
            (reverse_cut ? segment > 0 : segment + 1 < plan.segments)) {
            // Keep lookahead populated across chords; waiting for Idle here
            // would force a stop at every point on the ellipse.
            if (reverse_cut) segment--; else segment++;
            waiting_ack = false;
            emit();
            return;
        }
        if (!idle())
            return;
        waiting_ack = false;
        if (trace_enabled) {
            char done[180];
            snprintf(done, sizeof(done), "[P4DONE:PASS:%u|START:%u|STAGE:%s|X:%.6f|Z:%.6f]\r\n", pass + 1,
                     start + 1, names[stage], (double)sys.position[0] / settings.axis[0].steps_per_mm,
                     (double)sys.position[2] / settings.axis[2].steps_per_mm);
            hal.stream.write(done);
            if (stage == 7 && plan.indexed) {
                char sync[] = "P4SYNC", points[] = "P4SYNCTRACE";
                lathe_spindle_command(STATE_IDLE, sync);
                lathe_spindle_command(STATE_IDLE, points);
            }
        }
        if (stage == 0 && recovering) {
            recovering = false;
            stage = restart_entry ? 1 : 5;
            restart_entry = false;
        } else if (stage == 7 && reverse_cut) {
            stage = 5; // Retraced to the start: wait without retracting or advancing depth.
        } else if (stage == 10) {
            if (++start == plan.starts) {
                start = 0;
                pass++;
                lathe_critical_enter(&lock, 2000 + __LINE__);
                bool advance = advance_requested;
                advance_requested = false;
                lathe_critical_exit(&lock);
                if (advance && pass + 1 < plan.config.passes)
                    pass++;
            }
            stage = pass < plan.config.passes ? (plan.config.operation == LATHE_ELLIPSE ? 2 : 4) : 11;
            segment = 0;
        } else if (stage == 14) {
            message("Cycle complete", false);
            return;
        } else
            stage++;
    }
    if (idle())
        emit();
}
void lathe_cycle_poll(void)
{
    // UART writes can call the realtime hook while making room in the FIFO.
    // Keep a diagnostic/report from recursively advancing the same transition.
    static bool entered;
    if (entered)
        return;
    entered = true;
    lathe_follow_poll();
    poll_cycle();
    entered = false;
}
status_code_t lathe_cycle_command(sys_state_t state, char *line)
{
    if (!strcmp(line, "P4THREADENTRY")) {
        if (state != STATE_IDLE || !published.active || stopping || !plan.indexed ||
            stage != 7 || !thread_entry || lathe_axis_change_pending() || lathe_update_active())
            return Status_IdleError;
        return lathe_thread_entry_execute(lathe_cycle_depth(&plan, pass), plan.finish, plan.lead);
    }
    if (!strcmp(line, "P4ADVANCE"))
        return lathe_cycle_advance() ? Status_OK : Status_IdleError;
    if (!strcmp(line, "P4RELEASE")) {
        lathe_follow_release();
        return Status_OK;
    }
    if (!strncmp(line, "P4MANUAL=", 9)) {
        char axis, extra;
        int sign;
        double distance;
        unsigned held;
        if (sscanf(line + 9, "%c,%d,%lf,%u%c", &axis, &sign, &distance, &held, &extra) != 4 || held > 1)
            return Status_InvalidStatement;
        return lathe_follow_jog(axis, sign, distance, held) ? Status_OK : Status_IdleError;
    }
    if (!strncmp(line, "P4FOLLOW=", 9)) {
        lathe_follow_config_t c = {0};
        unsigned forward;
        char extra;
        if (sscanf(line + 9, "%u,%lf,%lf,%u,%lf,%lf,%lf,%lf%c", &c.mode, &c.pitch, &c.ratio, &forward,
                   &c.x_min, &c.x_max, &c.z_min, &c.z_max, &extra) != 8 ||
            forward > 1)
            return Status_InvalidStatement;
        c.aux_forward = forward;
        return lathe_follow_request(&c) ? Status_OK : Status_IdleError;
    }
    if (!strncmp(line, "P4CYCLETRACE=", 13)) {
        if (state != STATE_IDLE || lathe_cycle_busy())
            return Status_IdleError;
        if (strcmp(line + 13, "0") && strcmp(line + 13, "1"))
            return Status_InvalidStatement;
        trace_enabled = line[13] == '1';
        return Status_OK;
    }
    if (!strcmp(line, "P4CYCLE")) {
        lathe_cycle_status_t s;
        lathe_cycle_snapshot(&s);
        char text[180];
        snprintf(text, sizeof(text), "[P4CYCLE:ACTIVE:%u|PASS:%u|START:%u|STAGE:%s]\r\n", s.active, s.pass,
                 s.start, s.message);
        hal.stream.write(text);
        return Status_OK;
    }
    if (strncmp(line, "P4CYCLE=", 8))
        return Status_Unhandled;
    if (state != STATE_IDLE)
        return Status_IdleError;
    lathe_cycle_config_t c = {0};
    unsigned thread, forward;
    char extra;
    if (sscanf(line + 8, "%u,%lf,%u,%u,%u,%lf,%lf,%lf,%lf,%lf%c", &thread, &c.pitch, &c.passes, &c.starts,
               &forward, &c.x_min, &c.x_max, &c.z_min, &c.z_max, &c.rpm_limit, &extra) != 10 ||
        thread > LATHE_ELLIPSE || forward > 1)
        return Status_InvalidStatement;
    c.operation = thread;
    c.threading = thread == LATHE_THREAD;
    c.aux_forward = forward;
    return lathe_cycle_request(&c) ? Status_OK : Status_IdleError;
}
