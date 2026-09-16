/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint32_t version;
    int32_t mode, measure, pitch_type, pitch, move_step, passes, starts;
    float cone_ratio;
    // Uses a formerly zeroed reserved byte; old ui_v1 blobs default to Hold.
    uint8_t aux_forward, sound, jog_mode, reserved; // jog_mode: 0 Hold, 1 Single step
} lathe_preferences_t;
bool lathe_preferences_get(lathe_preferences_t *);
void lathe_preferences_set(const lathe_preferences_t *);
// Machine-step endpoints: X min/max, Z min/max. INT32_MIN/MAX mean unset.
void lathe_saved_limits_get(int32_t limits[4]);
void lathe_saved_limits_set(const int32_t limits[4]);
uint8_t lathe_saved_disabled_get(void);
void lathe_saved_disabled_set(uint8_t mask);
#ifdef __cplusplus
}
#endif
