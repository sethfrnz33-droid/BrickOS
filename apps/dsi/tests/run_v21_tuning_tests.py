from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
cmd=['gcc','-std=gnu11','-D_DEFAULT_SOURCE','-O2','-Wall','-Wextra','-Werror',
     '-Itests/stubs','-Isource','tests/test_v21_tuning.c',
     'source/tuner.c','source/wire.c','source/storage.c','source/dashboard.c','source/surface3d.c',
     '-lm','-o','/tmp/bts_v21_tuning']
subprocess.run(cmd,cwd=root,check=True)
subprocess.run(['/tmp/bts_v21_tuning'],cwd=root,check=True)
