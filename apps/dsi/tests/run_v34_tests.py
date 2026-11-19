#!/usr/bin/env python3
"""INI integration and configuration edits; host only, no ECU connected."""
import pathlib,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[1]
ini=pathlib.Path(sys.argv[1]).resolve() if len(sys.argv)>1 else None
if not ini or not ini.is_file():raise SystemExit('Pass the actual Proteus F7 INI path')
exe='/tmp/bts-dsi-v34-fields'
subprocess.run(['gcc','-std=gnu11','-D_DEFAULT_SOURCE','-O2','-Wall','-Wextra','-Werror',
 '-ffunction-sections','-fdata-sections','-Itests/stubs','-Isource','tests/test_v34_full_fields.c',
 'source/tuner.c','source/storage.c','source/dashboard.c','source/surface3d.c',
 '-lm','-Wl,--gc-sections','-o',exe],cwd=root,check=True)
subprocess.run([exe,str(ini)],cwd=root,check=True,timeout=45)
