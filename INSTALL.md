# Clean USB installation

Version 0.4.1 removes the previous product identity and uses the
`lathe_settings` NVS partition. A full P4 flash erase and installation replaces
the previous firmware, partition table and settings. No intermediate firmware
is needed. Do not use an application-only OTA update for this transition.

## Prepare

Bring the controller and a USB data cable. Use the P4 programming/debug USB port,
not the ESP32-C6 coprocessor's programming port. Keep the motor drives disconnected
or unpowered throughout installation and first-start checks. The normal firmware
permits axis control once startup is ready.

Install ESP-IDF 5.5.2, source its `export.sh`, and build from the repository root:

```sh
git submodule update --init dependencies/esp32 dependencies/grbl dependencies/trinamic
python -m pip install -r requirements-build.txt
idf.py build
```

Replace `USB_PORT` below with the actual connected P4 serial port. If automatic
bootloader entry fails, use the board's BOOT/RESET sequence, then retry. Confirm
that the programming tool identifies an ESP32-P4 before erasing.

## Erase and install

**This erases the P4's entire flash:** old applications and rollback slots,
partition table, Wi-Fi credentials, controller calibration/settings, saved
positions, work offsets, limits, disabled-axis state, UI preferences and files.
Defaults are then loaded from `main/machine.h` and the application. The separate
ESP32-C6 Wi-Fi coprocessor is not erased or reflashed by these commands.

```sh
idf.py -p USB_PORT erase-flash
idf.py -p USB_PORT flash monitor
```

`flash` installs the bootloader, partition table, factory application and initial
OTA metadata from the same build. Keep power connected through verification.
Exit the serial monitor with Ctrl+]. There is no old firmware left on the P4 to
roll back to after this erase; later OTA updates establish new rollback slots.

## First boot

1. Confirm version 0.4.1, working display/touch and successful driver startup.
2. Re-enter Wi-Fi through the local provisioning tool or USB workflow described
   in [OTA.md](OTA.md). Stored credentials were erased.
3. With drives still unpowered, inspect steps/mm, travel direction, enable
   polarity, speed/acceleration, encoder calibration and spindle settings.
4. Establish physical positions, work zeros and machining limits before any
   deliberate motion. Previously saved coordinates and disabled-axis settings
   are gone. Check stop behavior before cutting.

For an electrically disconnected diagnostic fixture, build and flash the
separate enable-locked variant instead:

```sh
idf.py -B build-bench -DSDKCONFIG=sdkconfig.bench -DLATHE_BENCH_ONLY=ON -DLATHE_OTA_REQUIRE_PAIRING=ON build
idf.py -B build-bench -p USB_PORT erase-flash
idf.py -B build-bench -p USB_PORT flash monitor
```

After this clean install, routine updates use the current uploader and the
`esp32-p4-lathe-controller` image identity; see [OTA.md](OTA.md). Keep this partition
layout for subsequent application-only updates.
