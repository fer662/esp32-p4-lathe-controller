/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "diagnostics.h"
void lathe_diagnostics_poll(bool idle); // Sole grbl task; no network or formatting.
void lathe_diagnostics_serve(int fd); // Network task; cached observations only.
void lathe_driver_snapshot(lathe_diagnostics_t *s);
void lathe_spindle_snapshot(lathe_diagnostics_t *s);
void lathe_tmc_snapshot(lathe_diagnostics_t *s, bool refresh);
