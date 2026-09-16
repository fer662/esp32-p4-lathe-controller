# 0.4.0 extraction and validation

The source baseline is the integrated application at
`37037c878cd1500a436f0e71d5a81f1fa3a899f1` (0.3.26). This repository starts a new
application history; upstream driver and core histories remain in their forks.

## Boundaries

- The ESP32/P4 fork owns the existing GPIO, GPTimer pulse engine, PCNT capture,
  RV32F interrupt wrapper and shared HAL diagnostics. Its minimal example uses
  unchanged upstream core and default serial/storage/spindle services.
- This application supplies callbacks for startup/OTA interlocks, axis-disable
  cancellation and persistence, spindle tracking, services and application commands.
  Its motion linker fragment contains only the application's spindle code; the
  driver carries its own interrupt/core fragment.
- Core is a separate Git pin. It contains the general fixes and optional native
  synchronization extensions already used by 0.3.26, including phase-gated thread
  entry. Continuous-phase support remains optional and is not enabled here.
- LVGL, machine limits, motor wiring, motion settings migration revision, NVS
  formats and partition offsets are retained. The forced C configuration header is
  shared by the driver and application to keep core structure layouts identical.

The rename changes application symbols, component and task names, the default
image identity and diagnostic protocol labels. The updated companion scripts
match those labels. The OTA uploader alone also supports the old receiver;
see [MIGRATION.md](MIGRATION.md) for the two-stage image-name transition.

Fresh dependency resolution required declaring three disabled optional LVGL 9
backend switches used by the display adapter's CMake checks. The UI remains on
LVGL 8.3.11. Four diagnostic integer formats and an OTA status buffer size were
corrected after enabling the normal compiler format checks.

## Checks

- Host suites execute cancellation, cycle commands/lifecycle/preview, enable
  policy, jog routing, saved-state restoration, settings upgrades, OTA pairing,
  speed override, spindle wait, native threading/entry and work-zero behavior.
- New compatibility assertions cover bounded image-name matching, unchanged
  partition addresses and sizes, and old/new paired and LAN OTA greetings.
- The actual LVGL widgets pass pointer-routing tests and render preview screens.
- ESP-IDF 5.5.2 builds the normal controller, disconnected bench and upgrade bridge.
  CI repeats builds with the pristine official SDK. Local SDK installations are
  not treated as the reproducibility authority.
- The shared driver has host tests for pulse/reset ordering, enable policy,
  disabled axes, application hooks, and timing-fault inhibition.

No device was flashed or commanded during these checks. They do not establish
connector pulse timing, physical stopping distance, loaded threading accuracy,
or successful OTA migration on hardware. Historical bench records under
`docs/history/` refer to earlier firmware, not this extracted build.

Before publishing, source and documentation are checked for credentials, private
paths/network identifiers and screenshot metadata/text. Build outputs, local
configurations, device logs, backups and provisioning data are excluded from Git.
