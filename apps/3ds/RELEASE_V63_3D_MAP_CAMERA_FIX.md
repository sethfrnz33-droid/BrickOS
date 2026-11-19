# Brick Tuner Suite v6.3 — 3D map top/bottom camera fix

## Changes

- Reworked `map3d_project` to use identical pixel-per-world-unit scale on the table X and Y axes. The old mixed 132/56 coefficients made straight overhead and underside views appear compressed.
- Orbited the map about its mid-height rather than the calibration's zero-height floor; this eliminates asymmetrical high/low height perspective magnification at the poles.
- Gradually reduced perspective distortion toward exact +90°/-90° pitch, producing undistorted top/bottom views without limiting available rotation.
- Uniformly fitted the entire table surface into the map viewport without rescaling either axis independently. Eye framing is computed without stereo disparity so left and right eyes get the same camera transform.
- Reduced the selected-cell overlay's vertical lift at the polar angles, since a 3D rise at 90° should appear as depth rather than as a fixed upward screen offset.
- No calibration value, ECU read/write routine, audio trigger, map heat colour, or slider behaviour was changed.

## Verification

- 42 host Python regression scripts passed, including new `tests/test_v63_polar_map_projection.py` (compiles and runs production projection C code at both polar angles and rotated yaw).
- Native 3DS GPU projection and physical stereo display still need console verification.

## Build

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```
