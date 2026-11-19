# Brick Tuner Suite v4.4 — Stereo, Navigation Legibility, Camera

Changes compared to v4.3:

- Fixed reversed 3DS stereoscopic parallax: selected Tune cards and highlighted map cells move **toward** the viewer, not into the display.
- Selected Tune card lettering is a second layer that projects farther forward than the card itself. Dashboard gauge lettering likewise sits in front of the corresponding gauge panel.
- The **three small Tune-screen gauges are deliberately recessed** behind the selectable Tune cards; the six main Dashboard gauges project forward.
- Enlarged the upper-screen INI submenu and dialog labels (0.42/0.43 -> 0.54) and use eight comfortably spaced list rows rather than cramming nine.
- Removed the white navigation transition and its page-change audio trigger. Startup progress is unchanged.
- Expanded 3D map pitch controls to -90 through +90 degrees, enabling straight overhead and straight underside views, via Circle Pad, D-pad, or C-stick; yaw is unrestricted. Calibration data and write controls are unchanged.

Verification: `TEST_REPORT_V44.md` records 23/23 host regression scripts passing, including compiled native tests for front/back disparity and map projection at +/-90 degrees. Native 3DS build, optical stereoscopy on actual displays, and ECU connection are NOT verified in this environment.

Build with MSYS2:

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```
