# Brick Tuner Suite v4.3 — Black grid and corrected 3D surface

## Actual source changes

- Removed artificial Z=0 perimeter extrusion, ground-outline projection and sidewall triangles from the 3D height-field. Only the actual INI-defined table samples form the colored triangles. The 4x4 native test now confirms exactly 18 true surface triangles, with no extra side geometry.
- The 3D view retains its five distinct heat-value bands, INI-sourced Z values, Circle Pad/C-stick rotation, and constant-table warning. Calibration editing and ECU writing/burn handling are unchanged.
- Added an elevated high-contrast selected-cell outline and reticle, with additional per-eye parallax, independent of calibration data.
- Increased map stereoscopic separation from 9.0 to 11.5 per unit depth at maximum slider state. Visual appearance can differ by console and viewer comfort; the 3D slider remains the controlling limit.
- Changed application UI colors to near-black backgrounds, neutral dark panels, light text, and cyan/green emphasis. Added a thin fading perspective grid behind upper/lower interface content.
- Raised the highlighted tuning category and floating home/tuning gauges in stereo, with visible drop shadows; kept the original TunerStudio tab icons and comparable label font sizes.
- Removed full-width decorative header bands from upper views and adjusted dense map, list and gauge layouts to use more of the available screen.
- Replaced the two-sided blue transition ribbons with a 115 ms upward moving, semitransparent white scan with a short trailing fade.
- Preserved measured Wi-Fi live update Hz, 3DS battery status, stale/held telemetry state, ECU faults, write safeguards, 10-second idle gauge fallback and the real 3D HOME Menu model/build scripts from v4.2.

## Validation

- Host gcc source syntax check.
- Complete Python regression suite including native host-compiled render tests, ECU protocol/write interlock tests, and Windows Python/CIA banner script checks.
- Native 3D surface test confirms a non-flat 4x4 real U16 map yields 18 triangles, multiple color bins, and distinct per-eye projections; a truly constant table reports itself.
- Real devkitARM compile, stereoscopic 3DS hardware rendering, live ECU behavior and final CGFX CIA banner generation **were not available to test in this environment**.

## Build (MSYS2)

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

Python 3.12+ remains necessary for the rotating 3D HOME Menu CGFX converter, as in v4.2.
