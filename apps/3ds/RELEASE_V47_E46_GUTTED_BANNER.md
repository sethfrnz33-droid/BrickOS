# Brick Tuner Suite v4.7 — Gutted E46 3D HOME Menu banner

- Replaced the old chenille wordmark and Blender conversion with the gutted E46 `.glb` supplied by the user.
- Kept the source model's 5,673 triangles and 26 original material slots to allow hardware performance testing.
- Downscaled embedded textures to HOME Menu size without changing exterior geometry.
- Prepared a continuous eight-second full-car rotation using a rigid animated parent node, no skinning.
- Added a same-triangle-count reduced-texture fallback for conversion-size failures and checked the 512 KiB CGFX limit.
- Removed Blender as a build requirement; no in-app tuning UI or ECU protocol changes.

The GLB assets and build scripts have been inspected, but CGFX output and device performance remain unverified here.
