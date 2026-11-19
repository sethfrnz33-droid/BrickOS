#!/usr/bin/env python3
"""Full Proteus F7 INI and 3D cell-spacing regressions (host only)."""
import sys,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
ini=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else None
if not ini or not ini.is_file():raise SystemExit('Pass path to Proteus F7 INI')
def compile_and_run(name,sources,args=()):
    exe='/tmp/bts-dsi-v29-'+name
    subprocess.run(['gcc','-std=gnu11','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror','-O2',
      '-ffunction-sections','-fdata-sections','-Itests/stubs','-Isource',*sources,
      '-lm','-Wl,--gc-sections','-o',exe],cwd=root,check=True)
    subprocess.run([exe,*map(str,args)],cwd=root,check=True,timeout=45)
compile_and_run('surface',['tests/test_v29_surface.c'])
compile_and_run('ui',['tests/test_v29_ui.c','source/tuner.c','source/storage.c','source/dashboard.c','source/surface3d.c'],[ini])
print('PASS v29: INI-driven curve/favorite/status/navigation and uniform 3D mesh regressions')
