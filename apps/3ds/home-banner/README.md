# Brick Tuner Suite — E46 3D HOME Menu banner (v4.7)

The installed CIA's Nintendo 3DS HOME Menu banner is now a real E46 3D model, not an image inside the tuning app.

- `e46_gutted_original.glb`: unchanged gutted car source (5,673 triangles, 26 materials, 15 images).
- `e46_home_banner.glb`: all 5,673 original triangles, resized source textures, 8-second 360-degree rigid rotation, centered and scaled for the HOME Menu reference camera.
- `e46_home_banner_lite.glb`: **same geometry**, smaller textures only; converter automatically tries it if primary CGFX exceeds 512 KiB or conversion fails.
- `prepare_e46_banner.py`: fully reproducible off-device asset preparation tool using Python 3 + Pillow; no Blender required.
- `build_3d_banner.sh`: uses pinned pycgfx to produce `banner.cgfx` and validates CGFX magic and size.

The `.glb` files are prepared in advance. The first CGFX build needs Python 3.12+, Git, internet access for pycgfx, and Python pip packages, plus your usual devkitARM and `.exe` CIA tools. Blender is no longer required.

Build from MSYS2:

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

The HOME Menu 3D banner displays when you highlight the installed CIA title, not when running the `.3dsx`. The tile image `icon.png` is separate and still two-dimensional.

**Limitations:** Actual `pycgfx` conversion, HOME Menu compatibility, the rotation animation, and frame rate remain unverified on real 3DS hardware. Animated banners can fail on actual hardware even if they work in emulators. Retain your previous working CIA while testing.

## v4.8: Corrected CGFX over-size condition

The previous 5,673 triangle models used `doubleSided: true` on all 26 glTF
materials. The pinned pycgfx converter duplicates vertex data and index data
for each double-sided material, which pushed both generated CGFX files above
512 KiB (856,776 and 767,688 bytes). The car is coherently wound (over 99%
face/vertex normal alignment on major components), so single-sided exterior
meshes are appropriate.

The build now tries `e46_home_banner_lite_single.glb` first, and
`e46_home_banner_tiny_single.glb` next. Both preserve the original geometry, texture
UVs, materials, colour and eight-second 360-degree animation while removing the
unnecessary backface duplication. `prepare_single_sided.py` recreates both.

**The actual CGFX output size still needs to be verified by the local converter.**
Do not assume success unless the script reports a CGFX file under 524,288 bytes.

## v4.9: Larger E46, connected 3DS, visible title

- `e46_diorama_full.glb`: E46 at 5.0 scale (previously 3.5), raised and initially angled for a larger, clearer HOME Menu view; rotating low-poly Nintendo 3DS handheld with two visible screens, D-pad, buttons and a curved physical cable; stationary title **BRICK ECU TUNER SUITE** in front of the HOME Menu camera.
- `e46_diorama_tiny.glb`: identical 3D scene and geometry, but smaller car textures to reduce CGFX size.
- `e46_diorama_lean.glb`: car wheel-rim detail reduced as a size fallback while **retaining the 3DS and cable**; car body, paint, lights, and tyres unchanged.
- `e46_diorama_safe.glb`: enlarged E46 and title, without the handheld/cable; last-resort reduced-size option.
- `prepare_e46_diorama.py`: reproduces these models from the preserved v4.8 single-sided GLBs (requires NumPy, trimesh and Pillow on a development PC only). The normal CIA build uses the already prepared GLBs.

The build attempts full, tiny, lean, safe, and then the previous working E46-only model if all three new scenes exceed the Nintendo 3DS **524,288-byte CGFX** maximum. It prints which model was accepted and records that filename in `home-banner/banner_source.txt`. A fallback to an older E46-only model means the handheld/title were omitted; it must not be mistaken for a successful v4.9 diorama.

**Important:** The GLB geometry and build scripts have been validated offline; CGFX conversion and in-console appearance require a real devkit build to verify. Keep your previous working CIA in case the new banner has graphical issues.
