#!/usr/bin/env python3
import pathlib,subprocess
root=pathlib.Path(__file__).resolve().parents[1]
cmd=['gcc','-std=c11','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror',
     '-Itests/stubs','-Isource','tests/test_link_state.c',
     'source/wire.c','source/tuner.c','source/storage.c','source/dashboard.c','-lm','-o','/tmp/bts_link_state_test']
subprocess.run(cmd,cwd=root,check=True)
subprocess.run(['/tmp/bts_link_state_test'],cwd=root,check=True)
