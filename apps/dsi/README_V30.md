# Brick Tuner Suite DSi v30 — distinct flat 2D editors and optional 3D surfaces

Only `apps/dsi` / `DSi` is changed. The 3DS source and ESP32 firmware are not changed.

- **`[CurveEditor]` is always a flat 2D X/Y line plot**, using the real X-bin numerical spacing rather than evenly spacing points by index. It can never enter 3D surface mode.
- **`[TableEditor]` opens as a flat 2D, top-down color grid by default** with numeric X/Y axes and values, including tables reached from INI settings panels or Favorites. This fixes the unintended 3D presentation of a normal 2D table link.
- For tables with at least two rows and columns, the lower-screen **VIEW 3D** button switches into the optional shaded surface; **VIEW 2D** switches back without changing ECU data or the selected cell.
- Live trace, editable cells, axis editing, Favorites, and the nonblocking protected burn process remain unchanged.

## Validation

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```

Host regression: `python3 tests/run_v30_tests.py /path/to/rusefi_proteus_f7.ini`, plus existing v29/v28 and protected-burn tests. This release has not been visually or physically tested on actual DSi hardware.
