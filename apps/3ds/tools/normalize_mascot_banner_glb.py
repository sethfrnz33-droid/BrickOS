#!/usr/bin/env python3
"""Repair missing GLB mesh names for pinned pycgfx without changing geometry."""
from __future__ import annotations
import argparse
import json
import struct
from pathlib import Path

def ensure_named_meshes(path: Path) -> bool:
    raw=path.read_bytes()
    if len(raw)<28: raise ValueError("GLB file truncated")
    magic,version,length=struct.unpack_from("<4sII",raw)
    if magic!=b"glTF" or version!=2 or length!=len(raw):
        raise ValueError("Invalid GLB 2.0 header/length")
    json_size,chunk_type=struct.unpack_from("<I4s",raw,12)
    if chunk_type!=b"JSON" or json_size%4 or 20+json_size>len(raw):
        raise ValueError("Missing or invalid GLB JSON chunk")
    model=json.loads(raw[20:20+json_size].decode("utf-8").rstrip(" \x00"))
    if not model.get("meshes"): raise ValueError("Mascot GLB has no meshes")
    changed=False
    nodes=model.get("nodes") or []
    for idx,mesh in enumerate(model["meshes"]):
        if isinstance(mesh.get("name"),str) and mesh["name"].strip(): continue
        name=next((node.get("name") for node in nodes
            if node.get("mesh")==idx and isinstance(node.get("name"),str)
            and node["name"].strip()),None)
        mesh["name"]=name or f"BrickECU_Mascot_Mesh_{idx}"
        changed=True
    if not changed:
        print("Mascot GLB mesh names: OK")
        return False
    chunk=json.dumps(model,ensure_ascii=False,separators=(",",":")).encode("utf-8")
    chunk+=b" "*(-len(chunk)%4)
    payload=raw[20+json_size:]
    if len(payload)<8 or payload[4:8]!=b"BIN\x00":
        raise ValueError("Missing binary chunk; model left unchanged")
    result=struct.pack("<4sII",b"glTF",2,20+len(chunk)+len(payload))
    result+=struct.pack("<I4s",len(chunk),b"JSON")+chunk+payload
    assert result[20+len(chunk):]==payload
    path.write_bytes(result)
    print(f"Repaired missing mascot mesh name(s): {path}")
    return True

if __name__=="__main__":
    parser=argparse.ArgumentParser()
    parser.add_argument("input",type=Path)
    ensure_named_meshes(parser.parse_args().input)
