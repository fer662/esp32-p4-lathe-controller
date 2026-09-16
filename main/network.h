#pragma once
#include "grbl/hal.h"
void lathe_network_start(void);
void lathe_network_poll(void);
status_code_t lathe_network_command(sys_state_t,char *);
