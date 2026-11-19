# Brick Tuner Suite v4.8 — E46 HOME Menu banner CGFX size correction

The v4.7 CGFX conversion produced 856,776 bytes (primary) and 767,688 bytes (lite); both exceed the hard 524,288-byte limit.

## Root cause

All 26 source glTF materials were marked double-sided. The pinned `skyfloogle/pycgfx` converter duplicates every material's vertices, normals, UVs, and face indices for double-sided meshes. The car mesh uses consistently aligned winding and normals, so these extra backfaces are not required for the exterior.

## Changes

- Added `e46_home_banner_lite_single.glb` and `e46_home_banner_tiny_single.glb`, with original 5,673 triangles, same UVs/animation with downsized banner textures, and `doubleSided: false` for every material.
- The CIA banner converter now attempts the one-sided lite model first and validates the *actual* CGFX output against `0x80000`.
- The source conversion script `home-banner/prepare_single_sided.py` can reproduce the banner inputs without Blender.
- Kept the original gutted GLB and prior prepared variants unchanged.

## Verification

GLB structure, material flags, mesh triangle counts, animation and local scripts verified; actual CGFX conversion and HOME Menu rendering cannot be run in this environment. The expected size reduction (~318 KiB) is based on the original GLB vertex/index byte counts, not a measured CGFX result. `build-cia.sh` will fail explicitly if the converter still exceeds 512 KiB.
