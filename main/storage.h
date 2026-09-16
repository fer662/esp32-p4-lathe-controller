#pragma once
#include "grbl/hal.h"
void lathe_storage_init(void);
void lathe_storage_hal(void);
void lathe_storage_upgrade_motion(void); // boot only, after settings/hardware initialization
void lathe_storage_poll(void);
bool lathe_storage_restore_position(int32_t (*position)[N_AXIS]);
bool lathe_storage_ready(void);
status_code_t lathe_storage_command(sys_state_t,char *);
bool lathe_storage_wifi(char ssid[33],char password[65]);
bool lathe_storage_set_wifi(const char *ssid,const char *password);
bool lathe_storage_reject_next_ota(bool reject);
bool lathe_storage_consume_ota_rejection(void); // boot only, before controller tasks
