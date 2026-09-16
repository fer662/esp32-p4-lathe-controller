# esp32-p4-lathe-controller

Touchscreen lathe controller for the Waveshare ESP32-P4 Wi-Fi 6 10.1-inch board,
using grblHAL for planning, coordinated motion and spindle synchronization.

Version **0.4.1** uses only the `esp32-p4-lathe-controller` identity. The
application, reusable P4 driver and core remain separate, independently pinned
repositories. Installation over earlier firmware requires a clean USB flash;
old settings are intentionally erased. This version has not been installed on a
machine.

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
python -m pip install -r requirements-build.txt
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
in `dependencies.lock`; the component manager is pinned in `requirements-build.txt`
to make fresh resolution reproducible. Keep the three LVGL 9 optional-backend switches disabled;
the interface deliberately uses LVGL 8.3.11.

## Clean installation and updates

Follow [INSTALL.md](INSTALL.md) for the one-time USB erase and full flash. It
installs the new partition table, bootloader and application with the
`lathe_settings` partition. No intermediate image is required. The previous
firmware and all saved settings, Wi-Fi credentials and coordinates are erased.

After installation, [OTA.md](OTA.md) covers routine application updates, pairing
and rollback. The receiver accepts only `esp32-p4-lathe-controller` images and
the uploader uses only `P4OTA0` / `P4OTA1`. Device credentials, keys, backups, logs
and local configuration do not belong in Git.

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
geometry/preview, firmware identity, flash layout and OTA authentication. Native thread tests use ASan.
See [tests/ui_preview/README.md](tests/ui_preview/README.md) for rendering and
pointer-event tests of the real widgets. `verify_*.py` scripts are hardware bench
procedures, not automatic tests for a connected lathe.

External pulse timing, machine stop behavior and loaded threading still require
validation of the exact installed firmware. Historical results in
[docs/history](https://github.com/fer662/esp32-p4-lathe-controller/tree/603e61b54246906d3ccc517cc3bed075a08f20d7/docs/history) belong to earlier builds and are not certification of
this refactor. See [REFACTOR.md](REFACTOR.md) for the current changes and checks.

## License and origin

GPL-3.0-or-later; see [COPYING](COPYING). The Waveshare BSP retains its own license.
The application was extracted from `fer662/grblHAL-ESP32` at
`37037c878cd1500a436f0e71d5a81f1fa3a899f1` (0.3.26). UI provenance is recorded in
[components/lathe_ui/ORIGIN.md](components/lathe_ui/ORIGIN.md). Historical names are
retained only in source attribution and archived history.
