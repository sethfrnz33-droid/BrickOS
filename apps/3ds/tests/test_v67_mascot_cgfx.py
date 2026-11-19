#!/usr/bin/env python3
"""Verify mascot HOME CGFX inputs against pycgfx's named-mesh requirement."""
from pathlib import Path
import importlib.util
import json
import struct
import subprocess
import sys
import tempfile

root=Path(__file__).resolve().parents[1]
script=root/"tools/normalize_mascot_banner_glb.py"
spec=importlib.util.spec_from_file_location("mascot_glb",script)
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

def parts(raw):
    assert raw[:4]==b"glTF"
    assert struct.unpack_from("<II",raw,4)[0]==2
    assert struct.unpack_from("<I",raw,8)[0]==len(raw)
    size,typ=struct.unpack_from("<I4s",raw,12)
    assert typ==b"JSON"
    return json.loads(raw[20:20+size]),raw[20+size:]

def pack(model,data):
    chunk=json.dumps(model,separators=(",",":")).encode()
    chunk+=b" "*(-len(chunk)%4)
    return struct.pack("<4sII",b"glTF",2,20+len(chunk)+len(data))+struct.pack("<I4s",len(chunk),b"JSON")+chunk+data

glb=root/"home-banner/mascot_banner.glb"
if not glb.exists():
    subprocess.run([sys.executable,str(root/"tools/generate_mascot.py")],check=True)
before=glb.read_bytes()
model,binary=parts(before)
assert model["meshes"][0].get("name")
assert model["nodes"][0].get("name")
assert not module.ensure_named_meshes(glb)
assert glb.read_bytes()==before
assert "'name':'Brick ECU Mascot Billboard'" in (root/"tools/generate_mascot.py").read_text()
with tempfile.TemporaryDirectory() as directory:
    original=json.loads(json.dumps(model))
    original["meshes"][0].pop("name",None)
    old=Path(directory)/"legacy.glb"
    old.write_bytes(pack(original,binary))
    assert module.ensure_named_meshes(old)
    repaired,payload=parts(old.read_bytes())
    assert repaired["meshes"][0]["name"]
    assert payload==binary
    assert repaired["materials"]==model["materials"]
    assert not module.ensure_named_meshes(old)
banner=(root/"home-banner/build_3d_banner.sh").read_text()
assert 'tools/normalize_mascot_banner_glb.py home-banner/mascot_banner.glb' in banner
subprocess.run(["bash","-n",str(root/"home-banner/build_3d_banner.sh")],check=True)
subprocess.run(["bash","-n",str(root/"build-cia.sh")],check=True)
print("PASS named mascot mesh, legacy GLB repair, unchanged texture/geometry and shell syntax")
