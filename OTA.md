# Wireless updates and settings

## Flash layout

The clean-install layout has standard NVS at `0x9000`, PHY at `0xf000`, a 2 MiB
factory app at `0x10000`, two 3 MiB OTA apps at `0x210000` and `0x510000`, and
7 MiB of storage at `0x810000`. OTA metadata is at `0xf10000`; the 64 KiB
`lathe_settings` partition at `0xf12000` stores grblHAL/UI/Wi-Fi settings.
Nothing auto-starts motion after boot.

All boot-critical partitions are below 16 MiB. During implementation, an image
written across that boundary matched USB flash readback exactly but failed the
SDK's mapped checksum verification. Moving the executable slots alone was not
enough: the bootloader also failed to select OTA metadata above the boundary.
See the corresponding [Espressif issue](https://github.com/espressif/esp-idf/issues/18051).
This layout avoids enabling experimental 32-bit cache access.

## First installation

Follow [INSTALL.md](INSTALL.md) for the one-time full USB erase and flash of
version 0.4.1. This discards the old firmware, partition table and settings.
An application-only OTA update cannot install the new settings-partition label.
Subsequent updates keep this layout and preserve saved data.

## Wi-Fi

`wifi_provision.py PORT PRIVATE_JSON` reads `SSID` and `PASSWORD` from a local
JSON file and sends them over USB without logging them. Keep that file out of
Git. Credentials are stored in the separate P4 settings partition. Hosted Wi-Fi
uses the tablet's ESP32-C6 over SDIO with the original Waveshare pins. The
application pins `esp_hosted` 2.11.6 and `esp_wifi_remote` 1.3.2. The existing C6
firmware was retained and successfully connected; this OTA protocol updates the
P4 application, not the C6 coprocessor. Check host/slave compatibility before
upgrading those dependencies.

## Routine updates

Open Firmware Update on the touchscreen while the controller is stopped.
Firmware 0.3.4 makes pairing a CMake build option. This LAN build defaults to
`LATHE_OTA_REQUIRE_PAIRING=OFF`, and the panel says **Pairing disabled (LAN mode)**.
Upload without a key:

```sh
python ota_upload.py build/esp32-p4-lathe-controller.bin --host DEVICE_IP
```

To require a fresh temporary key again, build with pairing enabled:

```sh
idf.py -DLATHE_OTA_REQUIRE_PAIRING=ON build
```

Use `-DLATHE_OTA_REQUIRE_PAIRING=OFF` to disable it. The setting takes effect in the
newly installed image; the firmware currently running determines how that
installation itself authenticates. For a paired receiver, save the key displayed
on its touchscreen to a private local file:

```sh
python ota_upload.py build/esp32-p4-lathe-controller.bin --host DEVICE_IP --key-file PRIVATE_KEY_FILE
```

The uploader supports both current paired and LAN modes. With
USB attached it can still enter update mode and obtain pairing information:

```sh
python ota_upload.py build/esp32-p4-lathe-controller.bin --usb USB_PORT
```

TCP port 3232 accepts uploads only in the locally opened update session. Pairing
ON uses the existing `P4OTA1 <nonce>` greeting and a 68-byte header: big-endian
image size, SHA256 digest, then HMAC-SHA256 of nonce/size/digest. Authentication
happens before flash erase. Pairing OFF uses `P4OTA0` and a 36-byte size/digest
header, with no key or MAC: any reachable LAN client may upload during the
session. The uploader does not send an image to a paired receiver without a key.

Both modes require the complete image to match its SHA256 digest and pass project
and IDF image validation before selecting the boot slot. The digest detects
corruption; in LAN mode it does not authenticate the sender. Session expiry,
close/generation checks, motion ownership, boot confirmation and rollback remain
unchanged. Pairing keys, when enabled, expire on close or after five idle minutes.

Motion commands and new operations are refused during updates. Entry waits for
an empty planner and completed STEP pulse. UI settings and grbl NVS writes also
wait for idle; they do not share an active motion/OTA flash-writing window.
Interrupted uploads leave the current boot slot selected. Failed validation
leaves it selected too. Closing the panel during an accepted upload does
not interrupt the upload or release its motion lock. The handshake captures the
update session; accepting it and retaining motion ownership are serialized with
panel close and expiry. A handshake from a closed session cannot start erase.

A new OTA app starts with motion locked until driver initialization, the FPU ISR
canary, settings and touchscreen readiness pass. It confirms the app after five
seconds of successful readiness. Failure to become ready within 30 seconds
requests rollback. The bootloader also rolls back an unconfirmed app on reset.
`$P4OTATEST=REJECT` is an enable-locked bench test that deliberately rejects the
next OTA boot. `$P4OTATEST=CLEAR` cancels that test before uploading.

## Persistent state

The core's settings/coordinate blobs use its own CRC/versioned storage format.
UI preferences have a separate version: operation, units/pitch display, pitch,
step increment, passes, starts, cone ratio, infeed direction and sound. They
save after two seconds without another preference change and only while idle.
Saved positions, work offsets, limits and disabled-axis state can be restored
on ordinary boots, but a full flash erase deletes them. An armed operation does
not resume after reboot. This machine has no absolute axis position feedback;
verify physical position and bounds before machining, especially after a clean
installation.

`$P4STORE`, `$P4OTA`, `$P4AUDIO` and `$P4TMC` provide diagnostics. `$P4OTA` reveals
a pairing key only through the local USB interface while update mode is active;
do not publish those responses or private settings partition dumps.


## Reproducible bench checks

- `verify_persistence.py PORT`: temporary setting/UI change, reboot, verification
  and restoration of the original axis rate and normal bench UI defaults.
- `verify_ota.py PORT IMAGE`: authentication/digest/truncation failures, close
  during authentication, and motion ownership during an accepted upload.
- `verify_ota_boot.py PORT IMAGE --rollback`: deliberate new-image rejection and
  actual return to the previous boot slot.
- `verify_ota_boot.py PORT IMAGE`: alternate-slot installation and boot confirmation.

Run only on the disconnected, enable-locked tablet. Boot checks keep USB open
across reboot so they verify which partitions actually execute, then wait for
peripheral readiness; upload acceptance alone is not treated as successful boot.

## Working at the lathe without USB

The touchscreen supplies the update IP and temporary pairing key. Live encoder,
timing and TMC diagnostics use a separate read-only service so observation does
not acquire update mode or stop operation. See
[wireless commissioning](https://github.com/fer662/esp32-p4-lathe-controller/tree/603e61b54246906d3ccc517cc3bed075a08f20d7/docs/history/WIRELESS_COMMISSIONING.md).
