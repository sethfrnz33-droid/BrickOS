#!/usr/bin/env python3
"""Make Nintendo 3DS HOME Menu test GLBs from the unmodified, gutted E46.

Preserves every original exterior triangle and material assignment. Repackages
embedded image textures at banner scale, and adds a rigid single-node 360-degree
Y-axis animation. No Blender installation required.
"""
from __future__ import annotations

import copy
import io
import json
import math
import struct
from pathlib import Path
from PIL import Image

HERE = Path(__file__).resolve().parent
ORIGINAL = HERE / 'e46_gutted_original.glb'
MAGIC = b'glTF'


def read_glb(path: Path):
    raw = path.read_bytes()
    if len(raw) < 20:
        raise ValueError('Invalid GLB: too small')
    magic, version, total = struct.unpack_from('<4sII', raw)
    if magic != MAGIC or version != 2 or total != len(raw):
        raise ValueError('Invalid GLB header')
    pos = 12
    chunks = []
    while pos < len(raw):
        length, fourcc = struct.unpack_from('<I4s', raw, pos)
        pos += 8
        chunks.append((fourcc, raw[pos:pos + length]))
        pos += length
    if pos != len(raw):
        raise ValueError('Invalid GLB chunk length')
    j = json.loads(next(c for t, c in chunks if t == b'JSON'))
    b = next(c for t, c in chunks if t == b'BIN\x00')
    return j, b


def repack(mode: str, output: Path):
    doc, src = read_glb(ORIGINAL)
    doc = copy.deepcopy(doc)
    newbin = bytearray()

    def append_aligned(part: bytes) -> int:
        newbin.extend(b'\x00' * (-len(newbin) % 4))
        offset = len(newbin)
        newbin.extend(part)
        return offset

    image_view_ids = {image['bufferView']: i for i, image in enumerate(doc['images'])}
    for vi, view in enumerate(doc['bufferViews']):
        offset = view.get('byteOffset', 0)
        length = view['byteLength']
        payload = src[offset:offset + length]
        if len(payload) != length:
            raise ValueError(f'Buffer view {vi} extends beyond buffer')
        if vi in image_view_ids:
            idx = image_view_ids[vi]
            name = doc['images'][idx].get('name', '')
            img = Image.open(io.BytesIO(payload))
            # Hero bodywork remains recognizable at 3DS HOME Menu resolution.
            # The lite option is only used if CGFX exceeds the hardware cap.
            if mode == 'primary':
                limit = 128 if ('skin' in name.lower()) else 64
            elif mode == 'lite':
                limit = 64 if ('skin' in name.lower()) else 32
            elif mode == 'tiny':
                limit = 32 if ('skin' in name.lower()) else 16
            else:
                raise ValueError(f'Unknown texture quality: {mode}')
            if max(img.size) > limit:
                img.thumbnail((limit, limit), Image.Resampling.LANCZOS)
            # Ensure 3DS-compatible power-of-two dimensions.
            w, h = img.size
            def floor_pow2(v):
                return 1 << (v.bit_length() - 1)
            new_size = (max(4, floor_pow2(w)), max(4, floor_pow2(h)))
            if img.size != new_size:
                img = img.resize(new_size, Image.Resampling.LANCZOS)
            output_io = io.BytesIO()
            if doc['images'][idx]['mimeType'] == 'image/jpeg':
                img.convert('RGB').save(output_io, 'JPEG', quality=79, optimize=True)
            else:
                img.convert('RGBA').save(output_io, 'PNG', optimize=True)
            payload = output_io.getvalue()
        view['byteOffset'] = append_aligned(payload)
        view['byteLength'] = len(payload)

    # One rigid parent is safer for real HOME Menu hardware than skinning.
    # Scale and center the E46 for the approximately 44-unit reference camera.
    mesh_node = doc['nodes'][0]
    mesh_node['name'] = 'E46_BODY'
    parent_id = len(doc['nodes'])
    doc['nodes'].append({
        'name': 'E46_SPIN_ROOT',
        'children': [0],
        'translation': [0.0, -1.89, 0.0],
        'rotation': [0.0, 0.0, 0.0, 1.0],
        'scale': [3.5, 3.5, 3.5],
    })
    common_id = len(doc['nodes'])
    doc['nodes'].append({'name': 'COMMON', 'children': [parent_id]})
    doc['scenes'][doc.get('scene', 0)]['nodes'] = [common_id]

    # Make a rigid, looping rotation; no skin, bones or morph animation.
    # Quaternion keyframes 0 -> 360 degrees around the upright Y axis.
    seconds = [float(i) for i in range(9)]
    angle_offset = math.radians(35.0)
    rotations = []
    for i in range(9):
        angle = angle_offset + math.radians(45.0 * i)
        rotations.extend((0.0, math.sin(angle/2), 0.0, math.cos(angle/2)))
    time_bytes = struct.pack('<9f', *seconds)
    rot_bytes = struct.pack('<36f', *rotations)
    tv = len(doc['bufferViews'])
    toff = append_aligned(time_bytes)
    doc['bufferViews'].append({'buffer': 0, 'byteOffset': toff, 'byteLength': len(time_bytes)})
    rv = len(doc['bufferViews'])
    roff = append_aligned(rot_bytes)
    doc['bufferViews'].append({'buffer': 0, 'byteOffset': roff, 'byteLength': len(rot_bytes)})
    ta = len(doc['accessors'])
    doc['accessors'].append({'bufferView': tv, 'byteOffset': 0, 'componentType': 5126,
                             'count': len(seconds), 'type': 'SCALAR',
                             'min': [0.0], 'max': [seconds[-1]]})
    ra = len(doc['accessors'])
    doc['accessors'].append({'bufferView': rv, 'byteOffset': 0, 'componentType': 5126,
                             'count': len(seconds), 'type': 'VEC4'})
    doc['animations'] = [{
        'name': 'E46_RIGID_SPIN',
        'samplers': [{'input': ta, 'output': ra, 'interpolation': 'LINEAR'}],
        'channels': [{'sampler': 0, 'target': {'node': parent_id, 'path': 'rotation'}}],
    }]
    # Optional Blender extension not required for visible base-color materials.
    # Avoid unrecognized complex specular extensions in the CGFX converter.
    for m in doc.get('materials', []):
        extensions = m.get('extensions', {})
        extensions.pop('KHR_materials_specular', None)
        if not extensions:
            m.pop('extensions', None)
    doc['extensionsUsed'] = [e for e in doc.get('extensionsUsed', [])
                             if e != 'KHR_materials_specular']
    if not doc['extensionsUsed']:
        doc.pop('extensionsUsed', None)
    newbin.extend(b'\x00' * (-len(newbin) % 4))
    doc['buffers'][0]['byteLength'] = len(newbin)
    json_blob = json.dumps(doc, separators=(',', ':'), ensure_ascii=False).encode('utf-8')
    json_blob += b' ' * (-len(json_blob) % 4)
    data = (struct.pack('<4sII', MAGIC, 2, 12 + 8 + len(json_blob) + 8 + len(newbin))
            + struct.pack('<I4s', len(json_blob), b'JSON') + json_blob
            + struct.pack('<I4s', len(newbin), b'BIN\x00') + newbin)
    output.write_bytes(data)
    triangles = sum(doc['accessors'][p['indices']]['count'] // 3
                    for m in doc['meshes'] for p in m['primitives'])
    print(f'{output.name}: {len(data):,} bytes, {triangles:,} triangles, '
          f'{len(doc["materials"])} materials, {len(doc["images"])} textures, '
          f'{len(seconds)-1}s 360-degree rigid animation')
    return doc


if __name__ == '__main__':
    repack('primary', HERE / 'e46_home_banner.glb')
    repack('lite', HERE / 'e46_home_banner_lite.glb')
