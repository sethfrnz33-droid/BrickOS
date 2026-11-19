# Brick Tuner Suite v3.8 — 3D VE view, curves and readable two-screen tuning

## What changed

- The TunerStudio category tree on the upper LCD now keeps the same icon/text sizes while taking less horizontal space. RPM, CLT and AFR are stacked in the right-side strip on the upper LCD, always using received ECU data.
- Lower preview labels are larger, with six visible INI entries and highlighted selection. The bottom display shows six real channel gauges after 10 seconds without input on unmodified tuning screens; touching a control wakes the previous menu before acting. No staged edits, burn confirmations, power-cycle prompts or write operations are obscured.
- Open a 2D calibration map through its INI tuning-tree entry, then tap **3D VIEW** in the bottom editor. The upper display draws a real 3D height-field mesh from the verified ECU's map values, with perspective, color-shaded triangle facets and selected cell indication; the circle pad freely rotates it. Tap **2D VIEW** or press **A** to restore 2D editing. On a New 3DS, ZR can toggle where available. 3D is a VIEW-ONLY mode and cannot edit map values.
- Curve graphs have an always-visible Y scale (4 intervals), endpoint X values where the INI provides them, output units, a selected-point label with X and Y values, and a high-contrast selected marker. The graph's vertical range is based on the originally loaded ECU calibration rather than changing every time a point or precision step is edited.
- Fixed the Idle tab icon using the complete screenshot-provided original pictogram (the prior crop omitted the right side), corrected the decimal-dot baseline in the numeric glyph atlas, and increased dashboard numeral sizes.
- The rusEFI `e` error path now rejects non-text bytes instead of filtering them out, and requires two sequential IDENTICAL nonempty reports before showing a configuration error, while retaining the existing two-confirmed-clean-responses clearing requirement. This reduces spurious single-packet warnings; it does not mask persistent genuine ECU configuration faults.

## Hardware/software controls

- **Home:** upper 6 live gauges / Wi-Fi + battery + measured Hz; bottom Tune / Analyze / Logging / DS Settings.
- **Tune tree:** upper main TunerStudio groups + 3 compact gauges; bottom INI entries. A transfers focus top -> bottom, then opens the selected item on top.
- **Calibration map:** tap **3D VIEW** on the bottom LCD. Circle pad changes yaw/pitch; A or tap **2D VIEW** leaves 3D. Map edits remain restricted to the ordinary 2D editor and existing verified burn path.
- **Curve:** D-pad selects a point, SELECT enables local editing, L/R sets precision and A opens exact-number input when editing. Curve X/Y metadata and selected point values are displayed on both screens.
- **Idle view:** after 10s of inactivity on non-editing Tune pages, bottom screen switches to six live gauges. First button/touch wakes the existing controls, and requires another interaction to trigger any action.
- **ECU alerts:** only two matching, valid ECU error replies raise a fault. Fault details remain available with SELECT+Y. Power-cycle prompts stay A-acknowledged.

## Verification and limitations

- 17 automated host regression scripts pass, including transport-level mock fault tests and code-path checks for 3D viewer, curve ranges, safety and navigation.
- The 3D surface uses Citro2D geometry with CPU-side 3D perspective projection (not a pre-rendered fake/isometric background); unusually large maps are sampled to a maximum 20×20 render grid for performance, while the original editable data remains full resolution.
- No real 3DS compilation, console frame-rate measurement or ECU bench session was possible in the current environment. No ECU firmware or bridge protocol was modified.

## Build

Run in MSYS2 with devkitPro 3DS dependencies installed:

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

Extract the ZIP and merge its `BrickECU-3DS-Probe/` contents into your existing project folder before running the above.
