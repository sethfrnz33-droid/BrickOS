# Brick Tuner Suite v3.3 — Tuning, A/B Logs, SD Snapshots

**Project:** `BrickECU-3DS-Probe` (complete extracted folder).

## New functionality

1. **Offline A/B log comparison:** Open a saved log in Replay, hold **L + press X**, pick another saved CSV, and view a two-trace overlay with a matched channel. L/R cycles 9 channels; Left/Right scrubs the base log; X switches full/10s/30s window; Y picks another reference; START exports a uniquely named report. B returns to replay. Matching requires exact case-insensitive channel names; relative timestamps pair only recorded observations within 500ms; missing data remains missing. If recorded session metadata exists, mismatched ECU signatures are clearly warned about.
2. **Saved-data quality:** Replay -> START for Log Analysis -> SELECT for Data Quality. L/R selects recorded channels, A seeks to the end of the largest observed time gap, X exports a quality report, B goes back. Shows actual valid/missing counts, long intervals, duplicate timestamps, and unchanged-value runs. Does **not** diagnose ECU faults or invent sample values.
3. **Digital gauges:** L/R now cycles seven layouts, including one huge gauge and two huge gauges. In Gauge Settings, highlight a gauge row and use **X/Y** to set precision to AUTO / 0 / 1 / 2 / 3 decimal places. A still selects the ECU channel. Settings persist to the SD config.
4. **Map Tools:** New four-corner bilinear interpolation over an explicitly selected rectangular region; commit to LOCAL editor state as one undo frame only when INI limits/encoding accept all affected cells. Existing edit/burn protocol remains separate and unchanged.
5. **SD field snapshots:** The final two Map Tools actions save the loaded field's local editor values into `sdmc:/BrickECU/snapshots/field-####.bts` or compare against the most recent snapshot for the **same exact ECU signature + INI field + page + offset + byte count**. Snapshot loads are read-only; CRC mismatch and extra bytes are rejected. These files contain **one field only** (no whole ECU, map axes, or revision notes). Saved local editor values might be unburned; labels explicitly distinguish this from the ECU state.

## What did NOT change

- Existing ECU USB/TunerStudio protocol, calibration write and flash burn logic, INI loading, fault handling and bench trigger controls.
- There is no automatic map adjustment from logs or automatic writing from snapshots.

## Build command — MSYS2

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

Expected CIA after successful devkitPro build: `C:\Users\Seth\Downloads\BrickECU-3DS-Probe\BrickECU-3DS-Probe.cia`.

## Verification

See `TEST_REPORT_V33.md`. Host syntax and unit/simulation tests are **not** a devkitARM build, hardware test, or ECU flash validation.
