from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
cmd=['gcc','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-Isource',
    'tests/test_v21_dashboard.c','source/tuner.c','source/dashboard.c','-lm','-o','/tmp/bts_v21_dashboard']
subprocess.run(cmd,cwd=root,check=True)
subprocess.run(['/tmp/bts_v21_dashboard'],cwd=root,check=True)
s=(root/'source/main.c').read_text()
assert 'BTS_LIVE_POLL_FRAMES=2' in s
assert 'BTS_IDLE_TIMEOUT_FRAMES=600' in s
assert 'bt_async_live_is_new()' in s
assert 'if(idle_dashboard)bottom_idle_gauges();' in s
assert 'const BtField *field=bt_field(&def,m->target,false);' in s
print('PASS: v2.1 30Hz target, 10s wake protection, INI direct-field editor wiring')
