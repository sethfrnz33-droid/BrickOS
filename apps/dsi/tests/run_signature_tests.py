#!/usr/bin/env python3
import pathlib, subprocess
root=pathlib.Path(__file__).resolve().parents[1]
subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-Isource',
  'tests/test_signature.c','-o','/tmp/bts_signature_test'], cwd=root,check=True)
subprocess.run(['/tmp/bts_signature_test'],cwd=root,check=True)
