# v6.6 — MSYS build fix

The original v6.5 Makefile always invoked `python3` when building the mascot texture, despite the release ZIP already containing generated PNG assets. On Windows MSYS this fails when `python3` is not on PATH.

The project now builds `art/mascot.t3x` directly from the bundled `art/mascot/head.png`, `jaw.png`, and `mouth.png`, without invoking Python when those files exist. For optional seed regeneration or an asset-incomplete checkout, `tools/run_python.sh` locates installed `python3`, `python`, `py -3`, `py.exe -3`, or the Windows per-user Python executable. `build-cia.sh` uses that same launcher if its icon or GLB source is missing.

**Important:** The first-time HOME Menu **CGFX banner conversion** still requires Python 3.12+, Pillow, gltflib, and the pinned pycgfx converter. It is separate from compiling the `.elf` and `.3dsx`. A release containing the PNG assets removes the earlier `python3` failure but cannot promise a CIA build on a PC with no Python and no previously compiled `banner.cgfx`.

Build:

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

The asset-only target was tested without Python on PATH and the existing v6.4/v6.5 host tests passed. No live ECU or retail 3DS hardware validation has been performed here.
