/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { LATHE_TURN, LATHE_THREAD, LATHE_FACE, LATHE_CUT, LATHE_ELLIPSE } lathe_cycle_operation_t;

typedef struct {
    bool threading, aux_forward;
    unsigned passes, starts;
    double pitch, x_min, x_max, z_min, z_max, rpm_limit; // rpm_limit: ignored legacy protocol field
    lathe_cycle_operation_t operation;
} lathe_cycle_config_t;
typedef struct {
    double x, z, rpm, z_acceleration, z_max_rate, z_steps_mm;
    double x_acceleration, x_max_rate, x_steps_mm;
} lathe_cycle_machine_t;
typedef struct {
    lathe_cycle_config_t config;
    int direction, spindle_direction;
    double lead, cut_start, cut_end, approach, finish;
    double cut_acceleration; // Configured mm/s^2, reported in preview.
    double depth_start, depth_end, clearance, takeup;
    unsigned starts, segments;
    char cut_axis, depth_axis;
    bool indexed;
} lathe_cycle_plan_t;

// Pure geometry: millimeters are machine coordinates, X is radial. Cutting-axis
// targets stay inside the entered bounds, including Thread synchronization.
// Depth-axis clearance retracts are separate; this is not a homed travel envelope.
bool lathe_cycle_plan(const lathe_cycle_config_t *, const lathe_cycle_machine_t *, lathe_cycle_plan_t *, char *error, size_t size);
double lathe_cycle_depth(const lathe_cycle_plan_t *, unsigned pass);
void lathe_cycle_point(const lathe_cycle_plan_t *, unsigned pass, unsigned segment, double *x, double *z, double *feed);
const char *lathe_cycle_name(lathe_cycle_operation_t);
unsigned lathe_cycle_phase(const lathe_cycle_plan_t *, unsigned start);
unsigned lathe_cycle_phase_at(const lathe_cycle_plan_t *, unsigned start, double position, int spindle_direction);
unsigned lathe_cycle_segment_at(const lathe_cycle_plan_t *, unsigned pass, double x, double z);
#ifdef __cplusplus
}
#endif
