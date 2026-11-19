#!/usr/bin/env python3
"""Host structural regression for the real 3D, full-triangle-count E46 HOME banner."""
from pathlib import Path
import json,struct,subprocess,hashlib

root=Path(__file__).resolve().parents[1]
banner=root/'home-banner'
original=banner/'e46_gutted_original.glb'
assert original.is_file()

def parse_glb(path):
    raw=path.read_bytes()
    magic,v,n=struct.unpack_from('<4sII',raw)
    assert magic==b'glTF' and v==2 and n==len(raw)
    jl,jt=struct.unpack_from('<I4s',raw,12)
    assert jt==b'JSON'
    doc=json.loads(raw[20:20+jl])
    bl,bt=struct.unpack_from('<I4s',raw,20+jl)
    assert bt==b'BIN\x00'
    data=raw[28+jl:28+jl+bl]
    assert len(data)==bl
    ntri=sum(doc['accessors'][p['indices']]['count']//3 for m in doc['meshes'] for p in m['primitives'])
    return doc,data,ntri

orig,_,orig_count=parse_glb(original)
assert orig_count==5673,orig_count
for suffix in ('e46_home_banner.glb','e46_home_banner_lite.glb'):
    model=banner/suffix
    doc,binary,ntri=parse_glb(model)
    assert ntri==orig_count,(suffix,ntri)
    assert len(doc['images'])==15
    assert len(doc['materials'])==26
    assert len(doc['skins'])==0 if 'skins' in doc else True
    assert len(doc['animations'])==1
    a=doc['animations'][0]
    assert len(a['channels'])==1
    ch=a['channels'][0]
    assert ch['target']['path']=='rotation'
    target=ch['target']['node']
    assert doc['nodes'][target]['name']=='E46_SPIN_ROOT'
    sampler=a['samplers'][ch['sampler']]
    assert doc['accessors'][sampler['input']]['min']==[0.0]
    assert doc['accessors'][sampler['input']]['max']==[8.0]
    assert doc['nodes'][target]['scale']==[3.5]*3
    assert doc['scenes'][doc.get('scene',0)]['nodes']!=[0]
    # All GLB image sections remain valid after downsampling.
    for im in doc['images']:
        v=doc['bufferViews'][im['bufferView']]
        assert v['byteOffset']+v['byteLength']<=len(binary)
    assert model.stat().st_size<1_000_000

sh=(banner/'build_3d_banner.sh').read_text()
cia=(root/'build-cia.sh').read_text()
assert 'e46_home_banner_tiny_single.glb' in sh and 'e46_home_banner_lite_single.glb' in sh
assert '0x80000' in sh
assert 'find_blender' not in sh
assert 'makebanner -ci home-banner/banner.cgfx' in cia
assert 'extendedbanner' in cia
assert 'e46_diorama_tiny.glb' in cia and 'banner_source.txt' in cia
subprocess.run(['bash','-n',str(banner/'build_3d_banner.sh')],check=True)
subprocess.run(['bash','-n',str(root/'build-cia.sh')],check=True)
print('PASS: 5,673 real original triangles retained; 8s rigid animation; no Blender; 512 KiB CGFX protection')
