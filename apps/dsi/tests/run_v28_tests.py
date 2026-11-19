#!/usr/bin/env python3
"""DSi v28 hardware-independent INI menu index and 3D surface regressions."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1]
ini=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else None
if not ini or not ini.is_file():
    raise SystemExit('Usage: python tests/run_v28_tests.py /path/to/rusefi_proteus_f7.ini')
def build(name, sources,flags=()):
    exe='/tmp/bts-dsi-v28-'+name
    subprocess.run(['gcc','-std=gnu11','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror','-O2',
       '-ffunction-sections','-fdata-sections','-Itests/stubs','-Isource',
       *sources,'-lm','-Wl,--gc-sections','-o',exe,*flags],cwd=root,check=True)
    return exe
p=build('surface',['tests/test_v28_surface.c','source/surface3d.c'])
subprocess.run([p],cwd=root,check=True,timeout=30)
p=build('ui',['tests/test_v28_ui.c','source/tuner.c','source/surface3d.c','source/storage.c'])
subprocess.run([p,str(ini)],cwd=root,check=True,timeout=30)
p=build('help',['tests/test_v28_help.c','source/tuner.c'])
subprocess.run([p,str(ini)],cwd=root,check=True,timeout=30)
subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-O2',
  '-Itests/stubs','-Isource','-c','source/main.c','-o','/tmp/bts-dsi-v28-main.o'],cwd=root,check=True)
print('PASS DSi v28: host builds, readable menus, fast owner index, actual rotatable 3D surface, help index')
