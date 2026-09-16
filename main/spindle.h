/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "grbl/hal.h"
void lathe_spindle_profile(bool enabled);
float lathe_spindle_profile_rpm(void);
void lathe_spindle_init(void);
void lathe_spindle_ready(void);
void lathe_spindle_poll(void);
void lathe_spindle_idle(void);
void lathe_spindle_block(stepper_t *stepper);
void lathe_spindle_edge(axes_signals_t steps);
bool lathe_spindle_simulator_active(void);
float lathe_spindle_rpm(void); // grbl task only; same measurement used by the HAL
bool lathe_spindle_waiting_index(void); // grbl task only; no pulse output started yet
bool lathe_spindle_near_index(void); // short foreground polling window, no task delay
status_code_t lathe_spindle_command(sys_state_t state, char *line);

int64_t lathe_spindle_position(void);
void lathe_spindle_follow(bool enabled);
void lathe_spindle_follow_braking(void);

void lathe_spindle_entry_arm(double seconds, double lead, double acceleration);
bool lathe_spindle_entry_cutting(void); // Latched until the next entry is armed.
void lathe_spindle_entry_prepare(void); // Clear the previous pass latch before submitting another entry.
