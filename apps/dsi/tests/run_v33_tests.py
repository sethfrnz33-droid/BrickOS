#!/usr/bin/env python3
"""Host regression for DSi clutch-state-triggered LEGO sounds."""
import pathlib,subprocess,sys
root=pathlib.Path(__file__).resolve().parents[1]
if len(sys.argv)!=2 or not pathlib.Path(sys.argv[1]).is_file():
    raise SystemExit("Usage: run_v33_tests.py /path/to/rusefi_proteus_f7.ini")
exe='/tmp/bts-dsi-v33-clutch'
subprocess.run(['gcc','-std=gnu11','-D_DEFAULT_SOURCE','-O2','-Wall','-Wextra','-Werror',
                '-Isource','tests/test_v33_clutch.c','source/tuner.c','-lm','-o',exe],cwd=root,check=True)
subprocess.run([exe,str(pathlib.Path(sys.argv[1]).resolve())],cwd=root,check=True,timeout=45)
assets=root/'SD_Files'/'BrickECU'/'clutch'
for i in range(7):
    f=assets/f'{i:02d}.pcm8'
    assert f.is_file() and 32<=f.stat().st_size<=500000,f
print('PASS v33: all seven signed 8-bit audio assets present')
playexe='/tmp/bts-dsi-v33-audio'
subprocess.run(['gcc','-std=gnu11','-D_DEFAULT_SOURCE','-O2','-Wall','-Wextra','-Werror',
                '-Itests/stubs','-Isource','tests/test_v33_playback.c','source/sfx.c',
                '-o',playexe],cwd=root,check=True)
subprocess.run([playexe,str(assets.parent)],cwd=root,check=True,timeout=20)
