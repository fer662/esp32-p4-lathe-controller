/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "cycle_plan.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t position[3];
    float steps_per_mm[3];
    float work_offset[3]; // Core WCS + G92 + tool offset, in machine mm.
    uint8_t work_system; // Core coordinate-system ID: 0 = G54.
    float max_rate[3], acceleration[3];
    float rpm;
    uint32_t command_id, completed_id, stream_generation;
    uint32_t sampled_completed_id; // ACK observed by the last complete motion-state sample.
    int command_status, alarm;
    bool ready, moving, held;
    char state[24];
} lathe_status_t;

typedef struct {
    bool active;
    unsigned pass, start;
    char message[96];
} lathe_cycle_status_t;
bool lathe_cycle_request(const lathe_cycle_config_t *config); // thread safe, copied on submit
bool lathe_cycle_busy(void);
bool lathe_cycle_owns_stream(void);
void lathe_cycle_cancel(void);
bool lathe_cycle_advance(void);
void lathe_cycle_snapshot(lathe_cycle_status_t *status);
uint32_t lathe_bridge_cycle_submit(const char *line); // cycle service only
bool lathe_bridge_empty(void); // grbl task only
void lathe_bridge_discard_cycle_commands(void); // grbl task only
enum { LATHE_OWNER_PROFILE=1, LATHE_OWNER_FOLLOW=2, LATHE_OWNER_UPDATE=3 };
bool lathe_operation_claim(unsigned owner);
void lathe_operation_release(unsigned owner);
bool p4_motion_idle(void);
bool p4_motor_controls_enabled(void); // Build capability, not the instantaneous enable pin state.
void lathe_axis_set_disabled(char axis, bool disabled); // Thread-safe request; stop before removing torque.
bool lathe_axis_change_pending(void);
void lathe_bridge_init(void);
void lathe_bridge_flush(void); // grbl task only; discard requests on stream reset
void lathe_bridge_poll(void); // grbl task only
int32_t lathe_bridge_read(void); // grbl stream only, at a USB line boundary
bool lathe_bridge_active(void);
uint32_t lathe_bridge_submit(const char *line); // thread safe, nonblocking
void lathe_bridge_cancel(void); // cancels queued and active jogs, bypasses queue
void lathe_bridge_hold(void);
void lathe_bridge_resume(void);
void lathe_bridge_snapshot(lathe_status_t *status);
void lathe_ui_start(void);
bool lathe_ui_ready(void);
uint32_t lathe_ui_updates(void);
bool lathe_ui_screenshot(void (*write)(const char *));
bool lathe_ui_test_action(char action); // isolated bench diagnostic only
#ifdef __cplusplus
}
#endif
