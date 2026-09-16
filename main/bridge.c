/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "critical.h"
#include "freertos/queue.h"
#include "grbl/hal.h"
#include "grbl/protocol.h"
#include "grbl/state_machine.h"
#include "bridge.h"
#include "spindle.h"
#include "cycle.h"
#include "update.h"

// The core remains the sole parser/planner owner. UI messages enter its normal
// stream at complete-line boundaries; callbacks never execute G-code reentrantly.
typedef struct { uint32_t id, epoch; char text[120]; } request_t;
static atomic_uint operation_owner;
bool lathe_operation_claim(unsigned owner) {unsigned empty=0;return atomic_compare_exchange_strong(&operation_owner,&empty,owner);}
void lathe_operation_release(unsigned owner) {atomic_compare_exchange_strong(&operation_owner,&owner,0);}
static QueueHandle_t commands;
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static lathe_status_t published;
static uint32_t next_id, epoch, realtime_requests;
static request_t current;
static unsigned offset;
static bool active, acknowledge_error;
static uint32_t reporting_id;
static status_message_ptr previous_report;
static on_report_handlers_init_ptr previous_report_init;

static uint32_t submit(const char *line)
{
    if (lathe_axis_change_pending()) return 0;
    if (!commands || !line || strlen(line) >= sizeof(current.text) - 1 || strchr(line, '\n') || strchr(line, '\r')) return 0;
    request_t request;
    lathe_critical_enter(&lock, 1000 + __LINE__);
    request.id = ++next_id;
    request.epoch = epoch;
    lathe_critical_exit(&lock);
    snprintf(request.text, sizeof(request.text), "%s\n", line);
    if (xQueueSend(commands, &request, 0) != pdTRUE) return 0;
    return request.id;
}
uint32_t lathe_bridge_submit(const char *line)
{ return lathe_cycle_busy() || lathe_update_active() ? 0 : submit(line); }
uint32_t lathe_bridge_cycle_submit(const char *line) { return lathe_update_active()?0:submit(line); }
bool lathe_bridge_empty(void)
{ return !active && !acknowledge_error && !reporting_id && uxQueueMessagesWaiting(commands)==0; }
void lathe_bridge_discard_cycle_commands(void)
{
    // Service only calls this between parser reads; epoch invalidation discards
    // pending requests without truncating a partially delivered command.
    lathe_critical_enter(&lock, 1000 + __LINE__); epoch++; lathe_critical_exit(&lock);
}
void lathe_bridge_flush(void)
{
    lathe_cycle_reset();
    active = acknowledge_error = false;
    reporting_id = 0;
    lathe_critical_enter(&lock, 1000 + __LINE__);
    epoch++;
    published.stream_generation++;
    realtime_requests = 0;
    lathe_critical_exit(&lock);
}
void lathe_bridge_cancel(void)
{
    if (lathe_cycle_busy()) { lathe_cycle_cancel(); return; }
    lathe_critical_enter(&lock, 1000 + __LINE__);
    epoch++;
    realtime_requests |= 1;
    lathe_critical_exit(&lock);
}
void lathe_bridge_hold(void)
{ if (lathe_cycle_busy()) { lathe_cycle_cancel(); return; } lathe_critical_enter(&lock, 1000 + __LINE__); realtime_requests |= 2; lathe_critical_exit(&lock); }
void lathe_bridge_resume(void)
{ if (lathe_cycle_busy()) return; lathe_critical_enter(&lock, 1000 + __LINE__); realtime_requests |= 4; lathe_critical_exit(&lock); }
void lathe_bridge_snapshot(lathe_status_t *s)
{ lathe_critical_enter(&lock, 1000 + __LINE__); *s = published; lathe_critical_exit(&lock); }
bool lathe_bridge_active(void) { return active || acknowledge_error; }
int32_t lathe_bridge_read(void)
{
    lathe_critical_enter(&lock, 1000 + __LINE__);
    uint32_t generation = epoch;
    lathe_critical_exit(&lock);
    if (active && current.epoch != generation) {
        active = false;
        reporting_id = 0;
        return ASCII_CAN; // discard any partially supplied line, never truncate it into a move
    }
    if (acknowledge_error) {
        acknowledge_error = false;
        reporting_id = UINT32_MAX;
        return '\n'; // explicit acknowledgment of our own rejected UI command
    }
    if (!active) {
        while (xQueueReceive(commands, &current, 0) == pdTRUE) {
            if (current.epoch == generation) { active = true; offset = 0; break; }
        }
        if (!active) return SERIAL_NO_DATA;
        reporting_id = current.id;
    }
    unsigned char c = current.text[offset++];
    if (c == '\n') active = false;
    return c;
}
static status_code_t report_status(status_code_t status)
{
    if (!reporting_id) return previous_report(status);
    if (reporting_id != UINT32_MAX) {
        lathe_critical_enter(&lock, 1000 + __LINE__);
        published.completed_id = reporting_id;
        published.command_status = status;
        if (status != Status_OK) { epoch++; acknowledge_error = true; }
        lathe_critical_exit(&lock);
    }
    reporting_id = 0;
    return status;
}
static void bridge_report_init(void)
{
    if (previous_report_init) previous_report_init();
    previous_report = grbl.report.status_message;
    grbl.report.status_message = report_status;
}
void lathe_bridge_init(void)
{
    commands = xQueueCreate(16, sizeof(request_t));
    configASSERT(commands);
    previous_report_init = grbl.on_report_handlers_init;
    grbl.on_report_handlers_init = bridge_report_init;
    bridge_report_init();
}
void lathe_bridge_poll(void)
{
    lathe_critical_enter(&lock, 1000 + __LINE__);
    uint32_t rt = realtime_requests;
    realtime_requests = 0;
    lathe_critical_exit(&lock);
    if (rt & 1) protocol_enqueue_realtime_command(0x85);
    if (rt & 2) protocol_enqueue_realtime_command('!');
    if (rt & 4) protocol_enqueue_realtime_command('~');
    static uint32_t last_publish;
    uint32_t now = hal.get_elapsed_ticks();
    if (now - last_publish < 20 || !sys.driver_started) return;
    last_publish = now;
    lathe_status_t s = {0};
    hal.irq_disable();
    for (unsigned i = 0; i < 3; i++) s.position[i] = sys.position[i];
    hal.irq_enable();
    for (unsigned i = 0; i < 3; i++) {
        s.work_offset[i] = gc_get_offset(i, true);
        s.steps_per_mm[i] = settings.axis[i].steps_per_mm;
        s.max_rate[i] = settings.axis[i].max_rate;
        s.acceleration[i] = settings.axis[i].acceleration / 3600.0f;
    }
    s.work_system = gc_state.modal.g5x_offset.id;
    sys_state_t state = state_get();
    s.ready = sys.driver_started && lathe_ui_ready();
    s.moving = state == STATE_CYCLE || state == STATE_JOG || state == STATE_HOMING;
    s.held = state == STATE_HOLD;
    s.alarm = sys.alarm;
    s.rpm = lathe_spindle_rpm();
    const char *name = state == STATE_IDLE ? "Ready" : state == STATE_JOG ? "Jogging" : state == STATE_CYCLE ? "Running" : state == STATE_HOLD ? "Held" : state == STATE_ALARM ? "Alarm" : "Stopped";
    snprintf(s.state, sizeof(s.state), "%s", name);
    lathe_critical_enter(&lock, 1000 + __LINE__);
    s.stream_generation = published.stream_generation;
    s.command_id = next_id;
    s.completed_id = published.completed_id;
    s.sampled_completed_id = published.completed_id;
    s.command_status = published.command_status;
    published = s;
    lathe_critical_exit(&lock);
}
