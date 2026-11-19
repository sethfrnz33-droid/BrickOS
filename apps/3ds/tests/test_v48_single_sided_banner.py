#!/usr/bin/env python3
"""Prevent the E46 double-sided CGFX regression from returning."""
from pathlib import Path
import json,struct,subprocess
root=Path(__file__).resolve().parents[1]
folder=root/'home-banner'

def read(name):
 raw=(folder/name).read_bytes()
 assert raw[:4]==b'glTF' and struct.unpack_from('<I',raw,4)[0]==2
 assert struct.unpack_from('<I',raw,8)[0]==len(raw)
 n,kind=struct.unpack_from('<I4s',raw,12)
 assert kind==b'JSON'
 j=json.loads(raw[20:20+n])
 ln,kind=struct.unpack_from('<I4s',raw,20+n)
 assert kind==b'BIN\0'
 assert ln==len(raw)-(28+n)
 triangles=sum(j['accessors'][p['indices']]['count']//3 for mesh in j['meshes'] for p in mesh['primitives'])
 return j,triangles,raw[28+n:]

for old,new in [('e46_home_banner.glb','e46_home_banner_single.glb'),('e46_home_banner_lite.glb','e46_home_banner_lite_single.glb'),('e46_home_banner_tiny.glb','e46_home_banner_tiny_single.glb')]:
 a,n0,b0=read(old)
 d,n,b=read(new)
 assert n0==n==5673
 assert b==b0, 'Meshes, texture or animations changed: expected only material flags'
 assert len(a['materials'])==len(d['materials'])==26
 assert all(x['doubleSided'] is True for x in a['materials'])
 assert all(x['doubleSided'] is False for x in d['materials'])
 for key in ['nodes','meshes','animations','accessors','bufferViews','images','textures']:
  assert d[key]==a[key],key
 assert len(d['animations'])==1
 assert len(d['animations'][0]['channels'])==1
 assert d['animations'][0]['channels'][0]['target']['path']=='rotation'

s=(folder/'build_3d_banner.sh').read_text()
assert 'for source in home-banner/e46_diorama_full.glb home-banner/e46_diorama_tiny.glb' in s and 'e46_home_banner_lite_single.glb' in s
assert 'p.stat().st_size > 0x80000' in s
assert 'BANNER_READY=1' in s
cia=(root/'build-cia.sh').read_text()
assert 'e46_diorama_tiny.glb' in cia and 'banner_source.txt' in cia
assert 'makebanner -ci home-banner/banner.cgfx' in cia
subprocess.run(['bash','-n',str(folder/'build_3d_banner.sh')],check=True)
subprocess.run(['bash','-n',str(root/'build-cia.sh')],check=True)
print('PASS: same 5,673 triangles, geometry, images and animation; backface duplication disabled; CGFX size guarded')
