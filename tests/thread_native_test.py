#!/usr/bin/env python3
"""Exercise a single synchronized Z block in the pinned native planner/step ISR; no hardware."""
from pathlib import Path
import subprocess
import sys
import tempfile
root=Path(__file__).resolve().parents[1]
p4=root
with tempfile.TemporaryDirectory() as directory:
    binary=Path(directory)/'native-thread'
    config=p4/'main/machine.h'
    if '--without-preload' in sys.argv:
        config=Path(directory)/'machine.h'
        config.write_text((p4/'main/machine.h').read_text().replace('#define SPINDLE_SYNC_PRELOAD 1', '#define SPINDLE_SYNC_PRELOAD 0'))
    sources=[p4/'tests/thread_native_test.c',p4/'components/lathe_ui/cycle_plan.c']
    sources += [root/'dependencies/grbl'/name for name in ('planner.c','stepper.c','pid.c','nuts_bolts.c')]
    command=['cc','-std=c11','-g','-fsanitize=address','-ffunction-sections','-fdata-sections',
             '-I',str(root/'dependencies'),'-I',str(root/'dependencies/esp32/p4/boards'),'-I',str(p4/'main'),'-I',str(p4/'components/lathe_ui'),
             '-include',str(config),*[str(p) for p in sources],
             '-Wl,-dead_strip' if sys.platform=='darwin' else '-Wl,--gc-sections','-lm','-o',str(binary)]
    subprocess.run(command,check=True)
    subprocess.run([str(binary)],check=True)
