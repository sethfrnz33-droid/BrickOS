#!/usr/bin/env python3
"""v31 grouped View panels and realtime channel coverage from the actual INI."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1]
if len(sys.argv)!=2 or not Path(sys.argv[1]).is_file():
    raise SystemExit('Usage: python3 tests/run_v31_tests.py /path/to/rusefi_proteus_f7.ini')
exe='/tmp/bts-dsi-v31-views'
subprocess.run(['gcc','-std=gnu11','-D_DEFAULT_SOURCE','-O2','-Wall','-Wextra','-Werror',
    '-ffunction-sections','-fdata-sections','-Itests/stubs','-Isource',
    'tests/test_v31_views.c','source/tuner.c','source/storage.c','source/dashboard.c','source/surface3d.c',
    '-lm','-Wl,--gc-sections','-o',exe],cwd=root,check=True)
subprocess.run([exe,str(Path(sys.argv[1]).resolve())],cwd=root,check=True,timeout=45)
