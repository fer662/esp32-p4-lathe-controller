/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "freertos/FreeRTOS.h"
// Task-only application locks. Tags: 1xxx bridge, 2xxx cycle, 3xxx follow,
// 4xxx network, 5xxx storage, 6xxx diagnostics; suffix is the source line of the acquisition.
void lathe_critical_enter(portMUX_TYPE *lock, unsigned tag);
void lathe_critical_exit(portMUX_TYPE *lock);
void lathe_critical_reset(void);
void lathe_critical_report(void);

void lathe_critical_snapshot(uint32_t values[2]);
