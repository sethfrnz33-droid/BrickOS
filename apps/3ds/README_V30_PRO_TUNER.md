# Brick Tuner Suite v3.0 — Professional Tuning Workspace

This version intentionally removes the 3D Pikmin/model-viewer launcher. The upper screen shows **actual live ECU gauges** on every ordinary navigation page. Dedicated maps, live graphs, replay, knock, trigger and channel views replace gauges only while those tools are open.

- Dark, high-contrast, alpha-layered dashboard with chamfered/rounded panels.
- Four large, touch-friendly navigation rows per page, one row per D-pad step.
- Category-specific vector icons, chosen from the INI folder description (fuel, ignition, triggers, sensors, throttle, boost, communications, graphs, logs, etc.).
- Five digital-only gauge layouts, changed with L/R and saved on SD. The former Pikmin layout number migrates to the readable four-gauge view.
- Heatmaps color **only cell backgrounds**; all numbers use the same light text color.
- Maps show at most eight columns and seven rows simultaneously to keep legible values. D-pad still pans to every calibration cell.
- Existing ECU communications, signed writes and burn verification, trigger controls, fault polling, and logs are retained from v2.9.

## Build in MSYS2

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

## Limitations

This ZIP contains source and assets; a 3DS `.cia` build and real ECU test must be done with devkitPro hardware/toolchain. Native host syntax and regression tests do not prove 3DS display readability or frame rate. 
