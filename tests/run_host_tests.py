#!/usr/bin/env python3
"""Run host-only regressions. Never invokes verify_*.py, serial, or OTA tools."""
from pathlib import Path
import subprocess
import sys
import tempfile
root = Path(__file__).resolve().parents[1]
for test in sorted((root / 'tests').glob('*_test.py')):
    print(f"Running {test.name}", flush=True)
    subprocess.run([sys.executable, str(test)], check=True, cwd=root)
with tempfile.TemporaryDirectory() as directory:
    executable = Path(directory) / 'cycle-plan'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I', str(root / 'components/lathe_ui'),
                    str(root / 'tests/cycle_plan_test.c'),
                    str(root / 'components/lathe_ui/cycle_plan.c'),
                    '-lm', '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print('All host regressions passed.')
