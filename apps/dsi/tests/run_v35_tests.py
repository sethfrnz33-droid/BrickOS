#!/usr/bin/env python3
"""Host regression for explicit DSi INI replacement (no ECU or DSi hardware)."""
import os, pathlib, subprocess, tempfile
root=pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='bts-dsi-v35-') as work:
    exe=os.path.join(work,'test_v35_ini_replace')
    cmd=['gcc','-std=gnu11','-D_DEFAULT_SOURCE','-O2','-Wall','-Wextra','-Werror',
         '-ffunction-sections','-fdata-sections','-Itests/stubs','-Isource',
         'tests/test_v35_ini_replace.c','source/tuner.c','source/storage.c',
         'source/dashboard.c','source/surface3d.c','-lm','-Wl,--gc-sections',
         '-o',exe,f'-DBTS_HOST_CACHE_DIR="{work}/sd"']
    subprocess.run(cmd,check=True,cwd=root)
    subprocess.run([exe],check=True,cwd=root,timeout=20)
    subprocess.run(['gcc','-std=gnu11','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror',
        '-O2','-Itests/stubs','-Isource','-c','source/main.c',
        '-o',os.path.join(work,'main.o')],check=True,cwd=root)
