# Extraction and clean-install validation

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
match those labels. Version 0.4.1 removes the intermediate build option, legacy
image-name acceptance and old uploader greetings. The settings partition is now
`settings`. [INSTALL.md](INSTALL.md) describes a full USB erase/install,
which intentionally discards previous settings and makes the bridge unnecessary.

Fresh dependency resolution required declaring three disabled optional LVGL 9
backend switches used by the display adapter's CMake checks. The UI remains on
LVGL 8.3.11. Four diagnostic integer formats and an OTA status buffer size were
corrected after enabling the normal compiler format checks. Strict-C11 Linux
host builds also receive the standard pi constant when `math.h` omits `M_PI`.
GCC host-test warnings are corrected without suppressing them. Hook registration
is evaluated even when runtime assertions are disabled.

The component manager is pinned to 2.4.6, as used by the successful fresh local
configure. The official SDK image's bundled resolver replaced LVGL 8 with LVGL 9
during its Kconfig retry; CI checks that the pinned resolver leaves the lock file
unchanged.

## Checks

- Host suites execute cancellation, cycle commands/lifecycle/preview, enable
  policy, jog routing, saved-state restoration, settings upgrades, OTA pairing,
  speed override, spindle wait, native threading/entry and work-zero behavior.
- Identity assertions cover exact, bounded image-name matching, the new settings
  partition label, and unchanged partition addresses and sizes. OTA socket tests
  cover paired and LAN transfers, rejected authentication and corrupt images.
- The actual LVGL widgets pass pointer-routing tests and render preview screens.
- ESP-IDF 5.5.2 builds the normal controller and disconnected bench.
  CI repeats builds with the pristine official SDK. Local SDK installations are
  not treated as the reproducibility authority.
- The shared driver has host tests for pulse/reset ordering, enable policy,
  disabled axes, application hooks, and timing-fault inhibition.

No device was flashed or commanded during these checks. They do not establish
connector pulse timing, physical stopping distance, loaded threading accuracy,
or successful installation on hardware. Historical bench records remain in
[the pre-cleanup revision](https://github.com/fer662/esp32-p4-lathe-controller/tree/603e61b54246906d3ccc517cc3bed075a08f20d7/docs/history) and refer to earlier firmware.

Before publishing, source and documentation are checked for credentials, private
paths/network identifiers and screenshot metadata/text. Build outputs, local
configurations, device logs, backups and provisioning data are excluded from Git.
