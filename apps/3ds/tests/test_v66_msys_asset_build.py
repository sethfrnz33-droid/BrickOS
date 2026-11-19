#!/usr/bin/env python3
"""Makefile regression: provided mascot sprites must build without Python."""
from pathlib import Path
import os, shutil, subprocess, tempfile

project=Path(__file__).resolve().parents[1]
mk=(project/"Makefile").read_text()
cia=(project/"build-cia.sh").read_text()
assert 'python3 tools/generate_mascot.py' not in mk
assert 'art/mascot/head.png art/mascot/jaw.png art/mascot/mouth.png' in mk
assert 'tools/run_python.sh' in cia
assert 'python3 tools/generate_mascot.py' not in cia

with tempfile.TemporaryDirectory() as d:
    t=Path(d)
    a=t/"project"
    (a/"art/mascot").mkdir(parents=True)
    (a/"art/mascot.t3s").write_text("--atlas -f rgba -z auto\nmascot/mouth.png\nmascot/head.png\nmascot/jaw.png\n")
    for name in ("head.png","jaw.png","mouth.png"):
        (a/"art/mascot"/name).write_bytes(b"fixture PNG already generated")
    (a/"Makefile").write_text(mk)
    arm=t/"arm";arm.mkdir()
    (arm/"3ds_rules").write_text("")
    bin=t/"bin";bin.mkdir()
    for name in ("make","bash","rm"):
        (bin/name).symlink_to(shutil.which(name))
    tex=bin/"tex3ds"
    tex.write_text("#!/bin/sh\nwhile [ \"$1\" != '-o' ]; do shift; done\nshift\nprintf 'ok' > \"$1\"\n")
    tex.chmod(0o755)
    env=dict(os.environ,DEVKITARM=str(arm),PATH=str(bin))
    p=subprocess.run([str(bin/"make"),"-C",str(a),"clean","art/mascot.t3x"],
                     env=env,text=True,capture_output=True)
    assert p.returncode==0,p.stderr
    assert "python" not in p.stdout.lower()
    assert (a/"art/mascot.t3x").read_bytes()==b"ok"

for name in ("build-cia.sh","tools/run_python.sh","home-banner/build_3d_banner.sh"):
    subprocess.run(["bash","-n",str(project/name)],check=True)
print("PASS mascot PNG clean rebuild without any Python on PATH")
