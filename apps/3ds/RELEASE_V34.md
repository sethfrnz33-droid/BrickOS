# Brick Tuner Suite v3.4 — Visible-First Navigation

## The main changes

- Five-mode Home: Dashboard / Tune Maps / Logs / Analyze / Settings & Diagnostics; all five visible without scrolling.
- Workspaces provide a clear path to real implemented tools, not unimplemented placeholder buttons.
- Replay shows six touch controls instead of relying on hidden combinations: Play/Pause, Compare, Analyze, Trace Map, Export Report, and Quality.
- Map editor shows Tools, Compare Snapshot, Undo, and guarded Burn actions alongside numeric edit controls.
- Dashboard shows touch buttons for Layout, Stat Mode, Reset and Customize.
- One-tap map and log opening, return-to-parent routes, restored list position, and Select-to-Home convenience outside specialized screens.
- Existing ECU communication, trigger commands, write/burn safety/verification, and offline analysis code preserved; no new ECU command types were added.
- Existing roadmap/tracker retained (382 items; 139 implemented, 101 partial, 142 remaining in source audit). This navigation work does not artificially raise feature counts.

## Tests

See `TEST_REPORT_V34.md` for the exact completed suite. Host-side tests and source syntax checks pass. This is **not a verified devkitARM build** and **not a console or ECU hardware validation**.

## Build on MSYS2

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

The release ZIP contains the `BrickECU-3DS-Probe` directory; extract its contents into the existing project directory or replace the project with the enclosed directory before building.
