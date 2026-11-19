# Brick Tuner Suite v4.7 — Host verification

- All **25 project Python regression scripts passed** individually, including v4.7 GLB structure and HOME Menu packaging tests.
- `bash -n` succeeds for `build-cia.sh` and `home-banner/build_3d_banner.sh`.
- User-supplied gutted GLB kept unchanged (SHA-256 verified against attached file).
- Prepared primary GLB retains 5,673 triangles, 26 materials, 15 images, and a single rigid-node 8-second spin animation; the texture-reduced fallback retains the same triangles.
- Existing tuner UI and ECU write code unchanged from v4.6.

## Unverified

The pycgfx CGFX conversion and final `bannertool`/`makerom` CIA packaging were not run because this environment lacks the native 3DS toolchain, and the pinned pycgfx source is not locally available. The build automatically attempts both GLBs and fails clearly if neither yields a CGFX <=512 KiB. HOME Menu animation and hardware performance are unverified.
