# Brick Tuner Suite v3.1 — Trace Mode / Workflow Milestone

This is an incremental, complete source release, based on v3.0's clean dark tuning UI. **It does not implement the entire master feature wishlist.** All wishlist entries are tracked separately in `MASTER_FEATURE_ROADMAP.md` and `FEATURE_TRACKER.csv`.

## What works in this source

1. **Trace Mode, saved log → actual ECU map.** Open **Views & Instruments → Replay Saved Logs**, choose a CSV recording, stop at an interesting time, and press **Y**. A filtered list of maps opens. The list includes only INI maps whose `xchan` and, for 2D tables, `ychan` names exactly match channels recorded in that log. Choose a map with **A**. The ECU map is read normally, but the trace crosshair follows the *recorded* channel values, not a substituted live value. The editor does not fabricate a value when a recorded sample or an INI axis is missing. **B** returns to the trace-map list and then to replay.
2. **Map coverage from log.** When viewing a map opened from Trace Mode and before entering editing mode, press **X** to toggle the log-usage heatmap. Color backgrounds represent *valid recorded sample counts by nearest map cell*; white cell numbers always remain the real ECU calibration values. Processing is limited to 128 log samples per pass to avoid a single frame freeze. This is sample coverage, not an automated correction or a measure of sensor accuracy.
3. **Saved log bookmarks.** In Replay, **SELECT** adds or removes a bookmark at the cursor (near a previous bookmark within 250 ms), **D-pad UP/DOWN** goes to previous/next marker, **LEFT/RIGHT** seek by 1 s, **A** plays/pauses, **L/R** changes speed, and **X** opens statistics. Markers are stored in the same SD log folder as `<logname>.marks` and are reloaded whenever the log is opened. A marker cannot change ECU calibration.
4. **Local map edit Undo/Redo.** In a staged map or scalar field, **hold L + press X** to undo, **hold L + press Y** to redo. The history covers value and loaded axis-byte edits, retains up to 16 states, and clears after a successful verified burn or when another field is opened. Undo/redo changes only RAM, never the ECU. Normal X/Y adjustment and L/R step controls are preserved when not using those combinations.
5. **Existing functionality preserved.** Wi-Fi communication, INI cache, real gauge values, stale gauge handling, map editing, fault display, log recording, signed bench write/burn and verification, and guarded ECU controls are retained from v3.0; no new physical-output test command was added.

## Important constraints

- Trace Mode needs a saved 9-channel Brick Tuner CSV, a matching decoded INI, and a **currently connected ECU to read actual map bytes**. Log replay itself works offline. This is not an offline tune editor.
- To offer a table, the recorded log must include the **exact INI output channel identifiers** used by that table's axes (commonly `RPMValue`, `MAPValue`, but depends on the real INI). Before recording, assign those channels to gauge slots in the existing display setup. A table with missing recorded axes is not offered rather than guessing.
- Coverage bins use nearest axis cells, not multi-cell interpolation weights. Coverage counts indicate visits, not VE correction confidence. Live gauges are not populated with log data unless the user is viewing the explicit log replay screen.
- The number of samples and memory are bounded by the inherited application; 3DS hardware throughput, SD persistence and ECU compatibility must still be validated on a real console.
- No automatic air/fuel or ignition changes are made from analysis. Burn remains its existing separate verified operation.

## Build using MSYS2 on Windows

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

Install the generated `BrickECU-3DS-Probe.cia` with your existing installer. This release was **host-syntax-checked and simulator-tested**, but `devkitARM`, `tex3ds` and a physical 3DS are not available in this execution environment; it has not been compiled to a new `.cia` here.

## Development continuation

Every new release must update `MASTER_FEATURE_ROADMAP.md`, `FEATURE_TRACKER.csv`, `FEATURE_STATUS.md`, and add a tested release note. Prioritize P0 and P1 items, and never check an item merely because a UI button exists. The next recommended steps are full-project save/diff, region editing, log channel presets and validation on original 3DS hardware.
