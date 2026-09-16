# Rename and dependency migration

The current product and default firmware identity are `esp32-p4-lathe-controller`.
Application code and UI components use `lathe_` / `LATHE_`; shared driver code uses
`p4_` / `P4_`. The firmware version is 0.4.0.

## Existing 0.3.26 installations

The old OTA receiver accepts only the project name `h5_grblhal_p4`. It will reject
the new default image before writing its contents. For a one-time OTA transition,
build a bridge using the **same source** with the old image identity:

```sh
idf.py -B build-bridge -DSDKCONFIG=sdkconfig.bridge -DLATHE_LEGACY_OTA_BRIDGE=ON build
```

Its artifact is `build-bridge/h5_grblhal_p4.bin`. This bridge already runs the
renamed application and accepts both old and new project names. With the device
idle and local update mode open, the new `ota_upload.py` can upload it to old
firmware. Wait for reboot and successful boot validation before opening update
mode again and uploading `build/esp32-p4-lathe-controller.bin`.

Both receiver versions retain image digest validation and OTA rollback. New
firmware announces `P4OTA0` or `P4OTA1`; the new uploader also recognizes the
legacy `H5OTA0` and `H5OTA1` greetings. Pairing requirements are independent of
the rename. Use the new uploader with either version.

No upload is part of the refactor validation. The exact two-stage transition
still needs a device acceptance test. A bootloader/partition-table flash is not
required for this rename and must not be bundled with an application-only update.

## Saved data

The partition table is byte-for-byte unchanged in substance. The existing
`h5_settings` label is deliberately retained because OTA does not rewrite the
installed table. `main/compatibility.h` centralizes this legacy label and the
accepted image names. The NVS namespace, blob keys, schema versions, structure
layout, machine positions, work offsets, limits, disabled axes, calibration,
Wi-Fi credentials and migration revisions retain their existing representation.
No settings reset or new acceleration migration is introduced by this refactor.

Original source checkouts and their branches remain available; new development
belongs in this repository. The three Git submodules pin exact public revisions.
Do not advance them independently without rebuilding and running the host suites.
