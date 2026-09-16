/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "waveshare_p4_xz_map.h"
#ifndef LATHE_BENCH_ONLY
#ifdef P4_BENCH_ONLY
#define LATHE_BENCH_ONLY P4_BENCH_ONLY
#else
#define LATHE_BENCH_ONLY 0
#endif
#endif
// Core configuration belongs to this port, never to edits in the core submodule.
#define N_AXIS 3 // grblHAL retains XYZ indexing; the P4 driver rejects Y motion.
#define DEFAULT_LATHE_MODE 1
#define LATHE_UVW_OPTION 1
#define NGC_EXPRESSIONS_ENABLE 1
#define COMPATIBILITY_LEVEL 0
#define DEFAULT_X_STEPS_PER_MM 1200.0f
#define DEFAULT_Z_STEPS_PER_MM 200.0f
#if LATHE_BENCH_ONLY
#define DEFAULT_X_MAX_RATE 60.0f
#define DEFAULT_X_ACCELERATION 25.0f
#else
#define DEFAULT_X_MAX_RATE 300.0f // 5 mm/s for planned moves; manual jog still requests 60.
#define DEFAULT_X_ACCELERATION 25.0f
#endif
#define DEFAULT_Z_MAX_RATE 960.0f
#if LATHE_BENCH_ONLY
#define DEFAULT_Z_ACCELERATION 50.0f // Keep the disconnected benchmark baseline.
#else
#define DEFAULT_Z_ACCELERATION 100.0f
#endif
#define DEFAULT_STEP_PULSE_MICROSECONDS 10.0f
#define DEFAULT_STEP_PULSE_DELAY 5.0f
#define DEFAULT_STEPPER_IDLE_LOCK_TIME 255
#define DEFAULT_HOMING_ENABLE 0
#define DEFAULT_ENABLE_SIGNALS_INVERT_MASK 1
#define DEFAULT_DIR_SIGNALS_INVERT_MASK 4 // Positive direction: X low, Z high
#define DEFAULT_STEP_SIGNALS_INVERT_MASK 0
// Default application controls the real drives. An explicit bench build can
// still disable enables and include the disconnected-fixture diagnostics.
#if LATHE_BENCH_ONLY
#define BUILD_INFO "LATHE_P4_BENCH_MOTOR_ENABLES_LOCKED"
#else
#define BUILD_INFO "LATHE_P4_AXIS_CONTROLS_ENABLED"
#endif
#define P4_STEP_HZ 10000000UL

#define SPINDLE_SYNC_ENABLE 1
#define SPINDLE_SYNC_PRELOAD 1 // Phase wait before Thread X plunge; queued Z starts without reacquisition.
#define SPINDLE_SYNC_FEED_FORWARD 1
#define SPINDLE_SYNC_PATH_LIMITS 1
#define SPINDLE_SYNC_INDEX_ORIGIN 1
#define SPINDLE_SYNC_INDEX_TIMEOUT_MS 0 // External spindle may be stopped or hand turned.
#define SPINDLE_SYNC_MAX_RATE_FACTOR 1.0f // Retain physical axis maxima; no extra RPM headroom.
#define DEFAULT_SPINDLE_SYNC_P_GAIN 0.25f
#define DEFAULT_SPINDLE_PPR P4_ENCODER_CPR

// Keep AMASS interpolation interrupts within the display/Wi-Fi timing budget.
#define AMASS_CUTOFF_HZ 4000
