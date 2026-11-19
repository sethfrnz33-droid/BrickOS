from pathlib import Path
import subprocess
r=Path(__file__).resolve().parents[1]
subprocess.run(['gcc','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-Isource',
                'tests/test_v19_axes.c','source/tuner.c','-lm','-o','/tmp/bts_v19_axes'],cwd=r,check=True)
subprocess.run(['/tmp/bts_v19_axes'],cwd=r,check=True)
