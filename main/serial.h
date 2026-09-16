/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdbool.h>
bool lathe_serial_init(void);
void lathe_serial_poll(void);
unsigned lathe_serial_overflows(void);

bool lathe_serial_pending(void); // grbl task only, includes incomplete USB line
