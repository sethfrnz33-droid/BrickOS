> **Superseded by [v36](README_V36.md):** v35 deleted the old INI before verifying a new download; do not use this workflow. v36 preserves the prior valid INI until replacement succeeds.

# Brick Tuner Suite DSi v35 — clear and replace ECU INI

## On the Nintendo DSi

1. Open **Connection** from Home.
2. Press **START** (or tap **START: REPLACE STORED INI**).
3. Confirm the prompt with **A**, or cancel with **B**.
4. The application removes the old cached ECU definition from SD, invalidates its in-memory tuning tree and live channel offsets, rechecks the connected ECU identity through BrickOS, then requests and validates the replacement INI. The normal nonblocking download, parser, and SD snapshot process is reused.

The confirmation explicitly notes that this does **not** change ECU calibration, tune data, pins, or flash memory; it only deletes the DSi-side INI cache. A connected ECU and working BrickOS Wi-Fi bridge are required for automatic download. If offline, the app waits until the ECU is connected before downloading. Do not use ECU tuning functions until the replacement reports loaded/parsed. If SD deletion fails, the old in-memory definition is invalidated and the refresh does not start; check the SD card.

The action deletes only the following from `/BrickECU` if present:

- `ecu.ini` (downloaded raw definition)
- `ecu.meta`, `ecu.meta.tmp` (parsed metadata)
- `ecu.bak` (old definition backup)
- `ecu.tmp` (partial download)
- `ecu-unverified.ini` (unverified prior download)

Favorites, gauge layout preferences, audio, clutch settings, and other user files are preserved. The application will load a manually copied valid `/BrickECU/ecu.ini` on its next launch; copying a new definition alone does not guarantee it matches the ECU's actual firmware, so verify identity before writing.

## Verified

- `python3 tests/run_v35_tests.py`: confirmed old cache files removed, preferences retained, new definition parsed and all stale runtime tuning state cleared. Compiles the DSi main module against host stubs.
- v27 nonblocking protected burn and v28–v34 parser, UI, maps, Favorites and View regressions pass.
- The software hasn't been tested on a physical DSi or ECU in this environment; host tests do not demonstrate transfer reliability on real hardware.

## Build

```
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```

This is a DSi-only change. The 3DS and ESP32 firmware remain unchanged.
