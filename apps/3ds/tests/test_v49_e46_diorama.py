#!/usr/bin/env python3
"""Structural regression of the enlarged HOME Menu car + handheld + cable."""
import io,json,math,struct,subprocess
from pathlib import Path
import numpy as np
from PIL import Image
root=Path(__file__).resolve().parents[1]
folder=root/'home-banner'

def load(name):
    raw=(folder/name).read_bytes()
    assert raw[:4]==b'glTF' and struct.unpack_from('<I',raw,4)[0]==2
    assert struct.unpack_from('<I',raw,8)[0]==len(raw)
    n,magic=struct.unpack_from('<I4s',raw,12)
    assert magic==b'JSON'
    doc=json.loads(raw[20:20+n]);blen,magic=struct.unpack_from('<I4s',raw,20+n)
    assert magic==b'BIN\x00'
    buf=raw[28+n:28+n+blen];assert len(buf)==blen
    for acc in doc['accessors']:
        v=doc['bufferViews'][acc['bufferView']]
        assert 0 <= v.get('byteOffset',0)<len(buf)
    tri=sum(doc['accessors'][p['indices']]['count']//3 for m in doc['meshes'] for p in m['primitives'])
    return doc,buf,tri

old,_,old_tris=load('e46_home_banner_lite_single.glb')
assert old_tris==5673
for filename,expected_console in [('e46_diorama_full.glb',True),('e46_diorama_tiny.glb',True),('e46_diorama_safe.glb',False)]:
    d,buf,tris=load(filename)
    assert tris==5673+(500 if expected_console else 2),(filename,tris)
    rig=next((i for i,n in enumerate(d['nodes']) if n.get('name')=='E46_SPIN_ROOT'),None)
    assert rig is not None
    assert d['nodes'][rig]['scale']==[5.,5.,5.]
    assert d['nodes'][rig]['translation'][1]>-1.89
    assert len(d['animations'])==1
    samp=d['animations'][0]['samplers'][0]
    times=d['accessors'][samp['input']]
    assert times['min']==[0.0] and times['max']==[8.0]
    output=d['accessors'][samp['output']];v=d['bufferViews'][output['bufferView']]
    offset=v.get('byteOffset',0)+output.get('byteOffset',0)
    q0=np.frombuffer(buf,dtype='<f4',count=4,offset=offset)
    assert abs(float(q0[1])-math.sin(math.radians(55/2)))<1e-5
    assert all(not m.get('doubleSided',False) for m in d['materials'])
    assert len(d['images'])==len(old['images'])+1
    title=next(i for i,x in enumerate(d['nodes']) if 'stationary title' in x.get('name',''))
    assert title in d['scenes'][0]['nodes']
    assert title not in d['nodes'][rig]['children']
    view=d['bufferViews'][d['images'][-1]['bufferView']]
    im=Image.open(io.BytesIO(buf[view['byteOffset']:view['byteOffset']+view['byteLength']]))
    assert im.size==(256,48)  # v5.0 enlarged readable HOME Menu title
    assert im.getbbox(), 'HOME Menu title image should not be blank'
    if expected_console:
        children=[d['nodes'][i]['name'] for i in d['nodes'][rig]['children']]
        assert any('shell' in x for x in children)
        assert any('glass displays' in x for x in children)
        assert any('Wired connection' in x for x in children)
    assert (folder/filename).stat().st_size<450000

build=(folder/'build_3d_banner.sh').read_text()
assert 'e46_diorama_full.glb home-banner/e46_diorama_tiny.glb' in build
assert 'e46_diorama_safe.glb' in build and 'e46_diorama_lean.glb' not in build  # old handheld layout retired in v5.0
lean,lean_buf,lean_tris=load('e46_diorama_lean.glb')
assert 4600<lean_tris<5100 and any('Wired connection' in n.get('name','') for n in lean['nodes'])
assert len(lean['animations'])==1 and len(lean['images'])==16
assert 'p.stat().st_size > 0x80000' in build
assert "banner_source.txt" in build
cia=(root/'build-cia.sh').read_text()
assert 'e46_diorama_full.glb' in cia and 'banner_source.txt' in cia
assert 'makebanner -ci home-banner/banner.cgfx' in cia
assert 'extendedbanner' in cia
for sh in [root/'build-cia.sh',folder/'build_3d_banner.sh']:
    subprocess.run(['bash','-n',str(sh)],check=True)
print('PASS: larger E46, rotating 3DS+cable, stationary readable title, unchanged car geometry, protected CGFX conversion')
