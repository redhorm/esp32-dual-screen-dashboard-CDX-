#!/usr/bin/env python3
"""Host-only memory/undefined-behavior check of the production WOW state engine."""
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
UNITY = ROOT / '.pio/libdeps/native/Unity/src'
if not (UNITY / 'unity.c').exists():
    raise SystemExit('Run pio test -e native first to install Unity.')
with tempfile.TemporaryDirectory(prefix='storm-wow-sanitize-') as tmp:
    tmp = Path(tmp)
    flags = ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer']
    subprocess.run(['cc', *flags, '-I'+str(UNITY), '-c', str(UNITY/'unity.c'), '-o', str(tmp/'unity.o')], check=True)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', *flags, '-I'+str(UNITY), '-I'+str(ROOT/'src'), str(ROOT/'test/test_wow/test_main.cpp'), str(tmp/'unity.o'), '-o', str(tmp/'wow-tests')], check=True)
    subprocess.run([str(tmp/'wow-tests')], check=True)
print('Strict C++11 + AddressSanitizer + UndefinedBehaviorSanitizer: PASS. No hardware access.')
