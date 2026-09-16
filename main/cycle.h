/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "bridge.h"
#include "grbl/hal.h"
void lathe_cycle_poll(void); // only grbl task executes transitions
void lathe_cycle_reset(void);
status_code_t lathe_cycle_command(sys_state_t state, char *line);
