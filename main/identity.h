/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdbool.h>
#include <string.h>
#define LATHE_PROJECT_NAME "esp32-p4-lathe-controller"
// Install the matching partition table by USB when switching to this identity.
#define LATHE_SETTINGS_PARTITION "settings"
static inline bool lathe_ota_project_allowed(const char project_name[32])
{
    return strncmp(project_name, LATHE_PROJECT_NAME, 32) == 0;
}
