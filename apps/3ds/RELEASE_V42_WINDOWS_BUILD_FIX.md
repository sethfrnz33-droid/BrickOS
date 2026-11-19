# Brick Tuner Suite v4.2 — Windows/MSYS2 3D banner packaging fix

- Builds directly on v4.1; no changes to ECU data decoding, map rendering, or write safeguards.
- Fixes the MSYS2 Python detection issue by checking Windows `py.exe -3.12`, generic `py -3`, conventional Python commands, and standard per-user Windows Python locations.
- Requires **Python 3.12+ to be installed somewhere** for conversion. The conversion is **not precompiled**, and will print specific Windows installation instructions when Python is absent.
- Uses an isolated `home-banner/.banner-venv` for pycgfx dependencies to avoid modifying devkitPro's Python installation.
- Retains the user's actual `.glb` model for the rotating 3D HOME Menu banner, without using a flat image fallback.
- Removes unused `dream_center()`, `change_page()`, unused gauge unit and needle declarations; expands shutdown statements to remove the reported misleading-indentation warning.
- Native 3DS and final CGFX conversion not verified in this environment.

Run:

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```
