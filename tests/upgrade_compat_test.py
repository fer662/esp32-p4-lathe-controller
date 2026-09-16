#!/usr/bin/env python3
"""Check OTA rename acceptance and the installed flash-layout contract; no device I/O."""
from pathlib import Path
import csv
import io
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = r'''
#include <assert.h>
#include <string.h>
#include "compatibility.h"
int main(void) {
    char name[32] = LATHE_PROJECT_NAME;
    assert(lathe_ota_project_allowed(name));
    strcpy(name, LATHE_LEGACY_PROJECT_NAME);
    assert(lathe_ota_project_allowed(name));
    strcpy(name, "esp32-p4-lathe-controller-bad");
    assert(!lathe_ota_project_allowed(name));
    strcpy(name, "h5_grblhal_p4_bad");
    assert(!lathe_ota_project_allowed(name));
    memset(name, 'x', sizeof(name));
    assert(!lathe_ota_project_allowed(name));
    memset(name, 0, sizeof(name));
    assert(!lathe_ota_project_allowed(name));
    assert(!strcmp(LATHE_SETTINGS_PARTITION, "h5_settings"));
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / 'test.c').write_text(source)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address', '-I', str(root / 'main'),
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
# An OTA update cannot replace the device's partition table. Keep every installed
# label, offset and size, including the old data label, identical to 0.3.26.
expected = {
    'nvs': ('0x9000', '0x6000'), 'phy_init': ('0xf000', '0x1000'),
    'factory': ('0x10000', '2M'), 'ota_0': ('0x210000', '3M'),
    'ota_1': ('0x510000', '3M'), 'storage': ('0x810000', '7M'),
    'otadata': ('0xf10000', '0x2000'), 'h5_settings': ('0xf12000', '0x10000'),
}
lines = '\n'.join(line for line in (root / 'partitions.csv').read_text().splitlines()
                  if line.strip() and not line.startswith('#'))
actual = {row[0].strip(): (row[3].strip(), row[4].strip()) for row in csv.reader(io.StringIO(lines))}
assert actual == expected
print('PASS: new/legacy OTA identities, rejected impostors, bounded names, unchanged flash layout')
