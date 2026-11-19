# Brick Tuner Suite — development handoff (2026-11-19)

**Coordination note:** This document is published on a standalone audit branch. Another AI is actively editing BrickOS. Do not overwrite or merge unrelated work without inspecting current changes.

## Product and workflow

- Nintendo 3DS wireless tuning application: **Brick Tuner Suite**.
- Home: six live upper-screen gauges, Wi-Fi/battery/actual telemetry-Hz status, four lower-screen workspaces (Tune, Analyze, Logging, DS Settings).
- Tune navigation: upper screen has only TunerStudio-style root categories. Highlighting one previews its actual ECU INI entries on the lower screen. A moves focus to the lower screen; opening a setting shows its editor on top. No global "All Maps" shortcut.
- Keep RPM/CLT/AFR available in Tune; context help from INI `[SettingContextHelp]` is displayed only in settings/subfolder detail, not root categories.
- Map 2D and 3D views are determined by the selected INI entry. 3D aliases share calibration bytes with the 2D table; map orientation/perspective and axis decoding must not fabricate data.
- UI aims for legible text, compact headings, black perspective grid, restrained stereo depth, and motion-responsive rounded 3D gauges.
- ECU errors should reflect fresh ECU state; after an actual reconnection the UI must recheck and clear stale faults automatically.
- Logging, analysis, browser with delete confirmation, undo/redo, and embedded event audio/soundboard exist in the current v6.x source.
- Burns are distinct from edits: preserve verified reads, backups, ECU safe-state checks, write acknowledgements, flash completion, and post-burn verification. Burn audio stops before completion/failure audio.
- E46 3D HOME Menu banner is a separate CGFX asset, not an image inside the tuning app. It needs to remain below the 524,288-byte CGFX limit.
- The latest working source provided for this audit is local Brick Tuner Suite **v6.3**; changes described in the adjacent INI audit are proposed **v6.4**, not confirmed on 3DS hardware.

## Current INI-decoding findings

- Wrong axes can result from picking the first table sharing a Z field instead of the exact selected TunerStudio table.
- "RPM / LOAD" must not be hardcoded. Read `xyLabels`, `columnLabel`, axis channel/field names and units when supported.
- Validate X/Y axis array lengths against the selected Z table dimensions. Never invent axis values.
- Unresolved dynamic scale is unavailable data, **not** raw data with engineering units.
- General expressions, preprocessor macros, dynamic dimensions, and multi-series curves remain open work. See `INI_AXIS_AUDIT_2026-11-19.md`.

## Development and verification

Project directory and user-facing build command:
```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```
Deliver updates as a ZIP containing the `BrickECU-3DS-Probe/` folder. The source is in `source/main.c`. Host tests are under `tests/`, but passing them is not proof of native devkitARM or hardware correctness.

**Do not push the entire app source onto BrickOS main as part of this audit; another AI may have active edits.** Keep changes isolated and review before merging.
