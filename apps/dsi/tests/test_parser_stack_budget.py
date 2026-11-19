#!/usr/bin/env python3
"""Catch ARM9 parser stack regression; run with host GCC -O2."""
from pathlib import Path
import subprocess, tempfile, re
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
    output=Path(d)/'tuner.o'
    subprocess.run(['gcc','-std=c11','-O2','-Wall','-Wextra','-Werror',
                    '-fstack-usage','-Isource','-c','source/tuner.c','-o',str(output)],
                   cwd=ROOT,check=True)
    lines=list(Path(d).glob('*.su'))
    assert len(lines)==1,lines
    frames={}
    for line in lines[0].read_text().splitlines():
        fields=line.split('\t')
        if len(fields)<3:continue
        name=fields[0].split(':')[-1]
        frames[name]=int(fields[1])
    worst=max(frames.values())
    assert worst<=2048,(worst,frames)
    assert any('line_parse' in x for x in frames),frames
    print(f'PASS: INI parser stack frame <= 2KB (max {worst} bytes)')
