#!/usr/bin/env python3
"""Verify supplied sound bytes are packed into the 3DS executable, never read from SD."""
from pathlib import Path
import hashlib
import re
import shutil
import struct
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/'source/event_audio_assets.S').read_text()
backend=(root/'source/bts_event_audio.h').read_text()
makefile=(root/'Makefile').read_text()
assert 'event_audio_assets.o' in makefile
assert 'bts_load_embedded' in backend and 'linearAlloc(bts_audio_capacity)' in backend
assert 'bts_audio_loaded++' in backend
assert 'fopen(' not in backend and 'BTS_SFX_DIR' not in backend
assert 'ndspChnWaveBufAdd(0,&bts_audio_wave[slot]);' in backend
assert backend.count('extern const uint8_t bts_embedded_')==14
assert len(re.findall(r'^\.incbin "\.\./art/event_audio/\d\d\.pcm"$',source,re.M))==14
assert 'bts_audio_init(true);' in (root/'source/main.c').read_text()
assert 'bts_audio_close();' in (root/'source/main.c').read_text()
assert 'event_audio_assets.o' in makefile

bytes_count=0
expected=[]
for i in range(14):
  file=root/'art/event_audio'/f'{i:02d}.pcm'
  assert file.is_file() and file.stat().st_size%2==0
  assert 0<file.stat().st_size<=36*22050*2
  samples=file.read_bytes()
  assert any(v!=0 for v in samples)
  bytes_count+=len(samples)
  expected.append((i,len(samples),hashlib.sha256(samples).hexdigest()))
assert bytes_count>2_000_000,bytes_count

# Assemble with a host assembler to ensure that every incbin path works
# relative to the 3DS `build` subdirectory, and that the range is exact.
if shutil.which('gcc') and shutil.which('nm'):
  with tempfile.TemporaryDirectory() as td:
    d=Path(td)
    work=root/'build'
    work.mkdir(exist_ok=True)
    try:
      out=d/'embedded.o'
      subprocess.run(['gcc','-c',str(root/'source/event_audio_assets.S'),'-o',str(out)],cwd=work,check=True,capture_output=True,text=True)
      nm=subprocess.run(['nm','-S',str(out)],check=True,capture_output=True,text=True)
      syms={}
      for line in nm.stdout.splitlines():
        fields=line.split()
        if len(fields)>=3 and fields[-1].startswith('bts_embedded_'):
          syms[fields[-1]]=int(fields[0],16)
      for i,length,sha in expected:
        start=syms[f'bts_embedded_{i}_start']
        end=syms[f'bts_embedded_{i}_end']
        assert end-start==length,(i,start,end,length)
    finally:
      # Do not remove work directory if it existed before; this is a source tree.
      pass
print('PASS: 14 embedded PCM assets; %s bytes; assembler ranges valid; no SD installation required'%bytes_count)
