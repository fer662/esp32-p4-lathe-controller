/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { bool active,connected,pairing_required,validation_pending;char ip[20],key[33],message[96];unsigned percent; } lathe_update_status_t;
bool lathe_update_request(void);
void lathe_update_cancel(void);
bool lathe_update_active(void);
bool lathe_network_initialized(void);
void lathe_update_snapshot(lathe_update_status_t *);
#ifdef __cplusplus
}
#endif
