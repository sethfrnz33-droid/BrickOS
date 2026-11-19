#!/usr/bin/env python3
"""Remove unnecessary backface duplication from HOME Menu E46 banner variants.

pycgfx duplicates every mesh vertex and triangle when material.doubleSided is
true. Blender's export marked every material as double-sided even though this
E46 has outward facing, coherently wound exterior surfaces.
Keep geometry, winding, UVs, textures, animation, material assignments as-is.
"""
from __future__ import annotations
import copy
import json
import struct
from pathlib import Path
from PIL import Image
import io

ROOT = Path(__file__).resolve().parent
PAIRS = [
 ('e46_home_banner.glb','e46_home_banner_single.glb'),
 ('e46_home_banner_lite.glb','e46_home_banner_lite_single.glb'),
 ('e46_home_banner_tiny.glb','e46_home_banner_tiny_single.glb'),
]

def read_glb(path):
 raw=path.read_bytes()
 assert raw[:4]==b'glTF' and struct.unpack_from('<I',raw,4)[0]==2
 assert struct.unpack_from('<I',raw,8)[0]==len(raw)
 jlen,jtyp=struct.unpack_from('<I4s',raw,12)
 assert jtyp==b'JSON'
 doc=json.loads(raw[20:20+jlen])
 boff=20+jlen
 blen,btyp=struct.unpack_from('<I4s',raw,boff)
 assert btyp==b'BIN\0'
 blob=raw[boff+8:boff+8+blen]
 assert len(blob)==blen
 return doc,blob

def write_glb(doc,blob,path):
 jb=json.dumps(doc,separators=(',',':'),ensure_ascii=False).encode('utf8')
 jb+=b' '*((-len(jb))%4)
 blob+=b'\0'*((-len(blob))%4)
 path.write_bytes(struct.pack('<4sII',b'glTF',2,12+8+len(jb)+8+len(blob))+
                  struct.pack('<I4s',len(jb),b'JSON')+jb+
                  struct.pack('<I4s',len(blob),b'BIN\0')+blob)

def make_single(inname,outname):
 doc,blob=read_glb(ROOT/inname)
 count=0
 for material in doc['materials']:
  if material.get('doubleSided',False):
   material['doubleSided']=False
   count+=1
 write_glb(doc,blob,ROOT/outname)
 triangles=sum(doc['accessors'][p['indices']]['count']//3 for m in doc['meshes'] for p in m['primitives'])
 assert triangles==5673
 assert all(m.get('doubleSided') is False for m in doc['materials'])
 print(f'{outname}: {triangles} triangles, {count} double-sided materials fixed')

if __name__=='__main__':
 for src,dst in PAIRS:make_single(src,dst)
