from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
cmd=['gcc','-std=c11','-D_DEFAULT_SOURCE','-O2','-Wall','-Wextra','-Werror','-Itests/stubs','-Isource',
     'tests/test_v19_503.c','source/tuner.c','source/storage.c','source/wire.c','source/dashboard.c','-lm','-o','/tmp/bts_v19_503']
subprocess.run(cmd,cwd=root,check=True)
subprocess.run(['/tmp/bts_v19_503'],cwd=root,check=True)
