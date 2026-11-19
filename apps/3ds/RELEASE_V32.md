# Brick Tuner Suite v3.2 — Tuning Workflow Expansion

This is a complete 3DS **source project**. Existing BrickOS v4.8 communication protocol
and write/burn logic are unchanged. No `.cia` is precompiled by this environment.

## What actually changed since v3.1

### Map Tools: 15 local editor functions

Open an INI-defined table, and tap **MAP TOOLS** on the bottom screen.
Choose an action with D-pad/A, or tap twice. **B** returns to the calibration.

1. Mark a corner of the selection (B to map, move to opposite corner, reopen Tools).
2. Clear range selection (one cell selected).
3. Increase region by current numeric step.
4. Decrease region by current numeric step.
5. Increase region by 5%.
6. Decrease region by 5%.
7. Interpolate horizontally across the selection's two endpoints.
8. Interpolate vertically across the selection's two endpoints.
9. Smooth cells with four axis-adjacent neighbors, all computed from the pre-edit snapshot.
10. Restore selected values from the original ECU-loaded map.
11. Copy region to local clipboard.
12. Paste copied region at selected target cell (bounded, no partial overflow).
13. Toggle highlighting of changes relative to the ECU-loaded baseline.
14. Undo local edit.
15. Redo local edit.

L/R adjusts the numeric step while the Tools panel is open. A region edit validates **every**
new cell against the INI numeric encoding and configured ranges, and either commits the entire
local operation or rejects it without changes. Undo records one snapshot per successful operation.
All these controls work on a **local staged copy**, never auto-transmit calibration changes.

### Saved log analysis (new)

Open a saved CSV, press **START** while in Replay to open **LOG INSIGHTS**.

- L/R selects any of the nine recorded data channels, using its exact CSV header.
- X switches 12-bin histogram / actual time-series trace.
- Y chooses whole log, or the last 10 / 30 / 60 seconds ending at the replay cursor.
- Displays min, max, P05, median, P95, mean, valid/missing sample counts, recorded duration,
  longest measured sample interval, and long interval count (no invented packets).
- A jumps the replay cursor to the next *recorded* sample beyond the 5th/95th percentile.
- Down jumps to the largest observed sampling interval in the complete saved log.
- START saves a uniquely numbered `.txt` analysis report next to the CSV on the SD card.
- B returns to Replay. Y from Replay can still open the existing INI-matched map Trace view.

This analysis does **not** suggest automatic fuel/ignition changes or issue ECU commands.

### Digital gauge statistics

In the Dashboard screen, Y cycles **LIVE, MAX, MIN, AVG**. X resets captured statistics,
L/R cycles existing digital layouts. Historical statistics are calculated only from new,
valid ECU samples for each currently configured channel; reconnecting doesn't synthesize
readings. Other screens always display live/last-good gauges rather than historical modes.

### Session metadata and status

- CSV export now also saves a `<logname>.session.txt` file containing the ECU signature,
  cached INI signature, ESP32 build, sample count, duration, and channel list as captured at
  **recording start**. These values are not confused with current ECU connection state.
- A unique `.txt` analysis report can be generated later, without changing the CSV format.
- Recorder shows elapsed recording time and the measured incoming telemetry frequency.
- Bottom toolbar displays REC / BURN / EDIT / HELD where applicable.
- Upper dashboard now says **NO TELEMETRY** if the ECU connection exists but no valid data
  has arrived; it doesn't imply fresh gauges from merely being connected.
- In Trace Mode, A with editing off jumps the map cursor to the logged operating cell.

### Stability and preservation

- Existing v3.1 tuning, map tracing, ECU reads, guarded write/burn sequence, fault handling,
  3DS L/R gauge cycling, INI auto-sync, and trigger controls remain in the source.
- No extra remote actuation methods were added.
- Real device responsiveness, screen legibility, and saved-file behavior are NOT verified.
- `FEATURE_TRACKER.csv` contains all 382 features with `IMPLEMENTED`, `PARTIAL`, or `TODO`.
  Source-implemented is never the same as hardware-verified.

## Build on MSYS2 (devkitPro)

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

If building your existing folder, extract the single `BrickECU-3DS-Probe` folder from this
archive and replace the old folder contents. Keep any saved SD logs and calibration backups.

## Validation limitations

The source was checked with host GCC and original regressions using test stubs plus native
mock ECU tests. This environment **does not have** the devkitARM/3DS build toolchain or an
actual 3DS/ECU. Hardware control, UI performance, audio and flash behavior remain unverified.
