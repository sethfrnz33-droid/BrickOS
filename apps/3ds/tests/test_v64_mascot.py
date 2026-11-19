#!/usr/bin/env python3
"""v6.4: test exact mascot mouth geometry, PCM audio, 3DS HOME files, and host compilation."""
from pathlib import Path
import json,struct,subprocess,zlib
p=Path(__file__).resolve().parents[1]
subprocess.run(['python3',str(p/'tools/generate_mascot.py')],cwd=p,check=True,capture_output=True)

def rgba(path):
 data=path.read_bytes()
 assert data[:8]==b'\x89PNG\r\n\x1a\n',path
 pos=8;idat=b'';w=h=None
 while pos<len(data):
  n=int.from_bytes(data[pos:pos+4],'big');t=data[pos+4:pos+8];value=data[pos+8:pos+8+n];pos+=n+12
  if t==b'IHDR':w,h,depth,typ,*_=struct.unpack('>2I5B',value);assert (depth,typ)==(8,6)
  if t==b'IDAT':idat+=value
  if t==b'IEND':break
 raw=zlib.decompress(idat);rows=[];offset=0
 for y in range(h):
  assert raw[offset]==0, 'unfiltered test PNG expected';offset+=1
  rows.append(raw[offset:offset+w*4]);offset+=w*4
 return w,h,b''.join(rows)

w,h,head=rgba(p/'art/mascot/head.png')
_,_,jaw=rgba(p/'art/mascot/jaw.png')
_,_,full=rgba(p/'art/mascot/full.png')
_,_,mouth=rgba(p/'art/mascot/mouth.png')
assert (w,h)==(128,128)
assert all((head[i+3]!=0) != (jaw[i+3]!=0) or head[i+3]==jaw[i+3]==0 for i in range(0,len(head),4)), 'head & jaw must not overlap'
for i in range(0,len(full),4):
 assert (head[i:i+4] if head[i+3] else jaw[i:i+4])==full[i:i+4],f'closed face changed at pixel {i//4}'
assert any(mouth[i+3] for i in range(0,len(mouth),4))
# Mouth seam is below nose at 75% of the 3DS sprite; don't dislocate the jaw at the forehead.
assert all(jaw[4*(y*128+x)+3]==0 for y in range(0,96) for x in range(128))
assert any(jaw[4*(y*128+x)+3] for y in range(98,126) for x in range(128))
icon_w,icon_h,icon=rgba(p/'icon.png');assert (icon_w,icon_h)==(48,48)
assert icon[3]==0 and any(icon[i+3]>0 for i in range(0,len(icon),4))
banner_w,banner_h,banner=rgba(p/'home-banner/mascot_banner.png')
assert (banner_w,banner_h)==(256,128)
voice=(p/'art/mascot/voice.pcm').read_bytes();assert len(voice)%2==0 and 20000<len(voice)<100000
assert any(v for v in struct.unpack('<%dh'%(len(voice)//2),voice))
glb=(p/'home-banner/mascot_banner.glb').read_bytes()
magic,version,length=struct.unpack('<4sII',glb[:12]);assert (magic,version,length)==(b'glTF',2,len(glb))
jsonlen,jtype=struct.unpack('<I4s',glb[12:20]);assert jtype==b'JSON'
scene=json.loads(glb[20:20+jsonlen]);assert scene['materials'][0]['alphaMode']=='BLEND'
assert scene['images'][0]['mimeType']=='image/png'
source=(p/'source/main.c').read_text();header=(p/'source/bts_mascot.h').read_text()
assert '#include "bts_mascot.h"' in source
assert 'bts_mascot_draw(282,38,.82f)' in source
assert 'bts_mascot_say();' in source
assert 'ndspChnWaveBufAdd(2,&bts_mascot_wave)' in header
assert 'bts_mascot_jaw_gap' in header
assert 'post_ts(' not in header and 'arm_ecu' not in header
assert 'bts_mascot_shutdown();' in source
assert 'bts_mascot_atlas_start' in (p/'source/mascot_assets.S').read_text()
assert 'mascot_assets.o' in (p/'Makefile').read_text()
assert 'mascot_banner.glb' in (p/'build-cia.sh').read_text()
subprocess.run(['bash','-n',str(p/'build-cia.sh')],check=True)
subprocess.run(['bash','-n',str(p/'home-banner/build_3d_banner.sh')],check=True)
subprocess.run(['gcc','-std=gnu11','-Wno-implicit-function-declaration','-Wno-int-conversion',
 '-Wno-incompatible-pointer-types','-fsyntax-only','-I'+str(p/'tests/host_stubs'),
 '-I'+str(p/'source'),str(p/'source/main.c')],check=True,capture_output=True)
print('PASS mascot face seam, layers, jaw movement, PCM, GLB, dual icons, routing, host C syntax')
