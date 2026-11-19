# Brick Tuner Suite DSi v31 — grouped live View dashboards

This update changes **DSi only**, keeping `apps/3ds` and ESP32 firmware untouched.

## What changed

- Selecting **View → fuel_computer**, **View → ignition_state**, or another INI View target now opens one complete live panel instead of a submenu full of generic **Graph** items.
- The live panels use each `[UserDefined]` dialog’s `liveGraph` and `graphLine` definitions. Up to **four actual graph groups per page** appear as grouped readouts across both DSi screens (2 top, 2 bottom). All channels in each group show their current live value together.
- **X** switches the same four groups to real rolling multi-channel trend plots; **X** again returns to grouped numeric values. Lines are scaled independently to show trends, **not** comparable magnitudes between unlike units.
- **L/R or D-pad left/right** pages through further INI graph groups; pages of `[UserDefined]` indicators show read-only **ON/OFF/--** states. **B** returns to the same tuning-menu position or Favorites.
- The live poll byte range now includes all visible group channels and the dependencies of supported computed channels, rather than sampling only the two dashboard channels.
- The rendering is bounded to 20 active channels and 40 samples per channel; SD-derived formulas are cached on page changes rather than reparsed every frame.
- No ECU write protocol, 2D/3D table editor, 3DS code or ESP32 firmware changes.

## Source INI verification

Using the supplied `rusefi_proteus_f7.ini`, the host tests verify **103 liveGraph groups and 341 graphLine entries**, including four fuel-computer groups (15 channels), ten knock-control groups, live status indicators, page rotation, Favorites return and the 2D/3D editor plus protected-burn regression suites.

Hardware performance and live output decoding are **not** validated on a physical DSi. Unsupported computed expressions show `--` rather than invented values.

## Build in devkitPro MSYS2

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```

The resulting file is `DSi/Brick-Tuner-Suite-DSi.nds`.

## Host test

```bash
python3 tests/run_v31_tests.py /path/to/rusefi_proteus_f7.ini
```
