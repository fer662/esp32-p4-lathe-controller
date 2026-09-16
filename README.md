# esp32-p4-lathe-controller

Touchscreen lathe controller for the Waveshare ESP32-P4 Wi-Fi 6 10.1-inch board,
using grblHAL for planning, coordinated motion and spindle synchronization.

Version **0.4.0** separates the application from the reusable P4 driver and core.
It preserves the 0.3.26 operation logic, machine configuration, UI, wireless
updates and persisted state. This refactor has not been installed on a machine.

## Repository structure

| Location | Responsibility |
| --- | --- |
| `main/` | Application services, motion ownership, spindle/lathe policy, persistence, networking, OTA and TMC5160 setup |
| `components/lathe_ui/` | LVGL touchscreen, jog controls, cycle planning/preview and audio |
| `components/esp32_p4_wifi6_touch_lcd_x/` | Waveshare display, touch and audio BSP |
| `dependencies/esp32/` | Pinned [ESP32/P4 driver fork](https://github.com/fer662/grblHAL-ESP32) |
| `dependencies/grbl/` | Pinned [core fork](https://github.com/fer662/grblHAL-core) |
| `dependencies/trinamic/` | Pinned upstream Trinamic library |

The shared driver owns GPIO, pulse timers, PCNT feedback and the floating-point
interrupt boundary. `main/controller.c` supplies application callbacks for
startup/update interlocks, axis-disable policy, spindle tracking and diagnostics.
There is no private copy of the timer/GPIO driver or core in the application.
Core and driver pins are independent: the standalone driver still builds with
upstream core, while this application opts into the lathe synchronization changes.

## Build

Install ESP-IDF **5.5.2** with ESP32-P4 tools and source its `export.sh`.

```sh
git clone https://github.com/fer662/esp32-p4-lathe-controller.git
cd esp32-p4-lathe-controller
git submodule update --init dependencies/esp32 dependencies/grbl dependencies/trinamic
idf.py build
```

The normal build permits axis control. For a disconnected bench with motor enables
locked and fixture diagnostics compiled in:

```sh
idf.py -B build-bench -DSDKCONFIG=sdkconfig.bench -DLATHE_BENCH_ONLY=ON -DLATHE_OTA_REQUIRE_PAIRING=ON build
```

Application configuration is in `main/machine.h`; board wiring is selected through
`waveshare_p4_xz_map.h` from the driver dependency. Driver and application C code
compile with the same configuration header. Managed component versions are pinned
in `dependencies.lock`. Keep the three LVGL 9 optional-backend switches disabled;
the interface deliberately uses LVGL 8.3.11.

## Updating existing firmware

Read [MIGRATION.md](MIGRATION.md) before the first update. Firmware 0.3.26 rejects a
new image project name, so a one-time bridge image is required before installing
the normally named 0.4.0 image over Wi-Fi. Existing flash partitions and stored
settings are preserved. Do not replace the partition table as part of this rename.

[OTA.md](OTA.md) covers local update mode, the uploader, pairing and rollback.
The uploader supports both old and new firmware protocols. Device credentials,
keys, backups, logs and local configuration do not belong in Git.

## Operations and validation

[OPERATIONS.md](OPERATIONS.md), [ASSISTED_CYCLES.md](ASSISTED_CYCLES.md), and
[HAND_FOLLOW.md](HAND_FOLLOW.md) describe the inherited operation semantics.
Thread entry waits for spindle phase with X clear, then queues the native plunge
and cut without a foreground command gap. The refactor does not change geometry,
acceleration tuning or spindle-phase algorithms.

Run host regressions without a device:

```sh
python3 tests/run_host_tests.py
```

The host suites include actual planner/stepper threading execution, cycle
lifecycle/cancellation, jog behavior, axis disable, work coordinates, persistence,
geometry/preview, and rename/OTA compatibility. Native thread tests use ASan.
See [tests/ui_preview/README.md](tests/ui_preview/README.md) for rendering and
pointer-event tests of the real widgets. `verify_*.py` scripts are hardware bench
procedures, not automatic tests for a connected lathe.

External pulse timing, machine stop behavior and loaded threading still require
validation of the exact installed firmware. Historical results in
[docs/history](docs/history) belong to earlier builds and are not certification of
this refactor. See [REFACTOR.md](REFACTOR.md) for the current changes and checks.

## License and origin

GPL-3.0-or-later; see [COPYING](COPYING). The Waveshare BSP retains its own license.
The application was extracted from `fer662/grblHAL-ESP32` at
`37037c878cd1500a436f0e71d5a81f1fa3a899f1` (0.3.26). UI provenance is recorded in
[components/lathe_ui/ORIGIN.md](components/lathe_ui/ORIGIN.md). Historical names are
retained only in attribution, historical records and required upgrade compatibility.
