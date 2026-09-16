/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <stdbool.h>
#include <string.h>
#define LATHE_PROJECT_NAME "esp32-p4-lathe-controller"
#define LATHE_LEGACY_PROJECT_NAME "h5_grblhal_p4"
// Existing partition tables are retained by OTA. Do not rename their NVS label.
#define LATHE_SETTINGS_PARTITION "h5_settings"
static inline bool lathe_ota_project_allowed(const char project_name[32])
{
    return strncmp(project_name, LATHE_PROJECT_NAME, 32) == 0 ||
           strncmp(project_name, LATHE_LEGACY_PROJECT_NAME, 32) == 0;
}
