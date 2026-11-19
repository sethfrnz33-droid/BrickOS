#!/usr/bin/env python3
"""Build-script regression: Windows launcher fallback and clear missing-Python error."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
script = root / 'home-banner' / 'build_3d_banner.sh'
src = script.read_text()
assert 'try_python py -3.12' in src
assert 'try_python py.exe -3.12' in src
assert 'Programs/Python/' in src
assert '.banner-venv' in src
assert 'e46_home_banner_tiny_single.glb' in src and 'e46_home_banner_lite_single.glb' in src
assert 'banner.cgfx' in src
assert 'No flat PNG replacement' in src
subprocess.run(['bash', '-n', str(script)], check=True)

# Simulate an MSYS2 shell where Python and the Windows launcher are absent.
# Include only `dirname` and `cat` in PATH, so the script's diagnostic prints.
with tempfile.TemporaryDirectory() as td:
    bin_dir = Path(td) / 'bin'
    bin_dir.mkdir()
    for executable in ('dirname', 'cat'):
        (bin_dir / executable).symlink_to(f'/usr/bin/{executable}')
    cp = subprocess.run(['/bin/bash', str(script)], cwd=root,
                        env={'PATH': str(bin_dir), 'HOME': td},
                        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    assert cp.returncode == 1, (cp.stdout, cp.stderr)
    assert 'A usable Python 3.12+' in cp.stderr
    assert 'winget install --id Python.Python.3.12 -e' in cp.stderr
print('PASS: Windows Python launcher resolution and actionable missing-Python diagnostic')
