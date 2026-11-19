# Brick Tuner Suite v3.4 — discoverable tuning workspaces

This release reorganizes **the existing v3.3 application** around visibly labeled controls. It does not add, change, or remove ECU flash commands. All currently implemented calibration, logging, trace, and diagnostic facilities remain in the project.

## Main menu (bottom 320 × 240 LCD)

Five large buttons are visible simultaneously. The upper 400 × 240 LCD continues to show live digital gauges until a map, a log/analysis screen, or a dedicated diagnostic is opened.

| Visible mode | Visible destinations |
|---|---|
| Dashboard | Live dashboard, touch buttons: Next Layout / Live-Max-Min-Average / Reset Stats / Customize |
| Tune Maps | Open ECU Maps / All ECU Settings / Saved Favorites / Find Setting or Map |
| Logs | Record Live Log / Browse Saved Logs / Resume Loaded Replay / Live Trend Graphs / Live ECU Channels |
| Analyze | Analyze Loaded Log / Compare Two Logs / Trace Log to Map / Check Log Quality / Log Statistics |
| Settings & Diagnostics | ECU Setting Categories / ECU Errors & Conflicts / Connection Diagnostics / Gauge Layout & Channels / Monitor Live Channels / Knock Diagnostics / Quick Access Favorites / Find ECU Parameter / Trigger Diagnostics |

Workspaces use selectable scroll lists when they contain more items than fit on one screen. The selection is restored when returning to the parent. `SELECT` opens the five-mode workspace menu from ordinary screens. In map editing, `SELECT` continues to mean stage/unstage local edits and does not unexpectedly navigate away.

## Log review without shortcut combinations

**Logs > Browse Saved Logs > choose log** opens Replay. The bottom screen shows a scrub bar plus six directly touchable actions:

| Play/Pause | Compare | Analyze |
|---|---|---|
| Trace Map | Export | Quality |

- **Play/Pause** plays or pauses the current recorded session.
- **Compare** picks a second compatible saved CSV log.
- **Analyze** opens historical signal statistics and histograms.
- **Trace Map** lists tables that have matching recorded axis channels. No axes are guessed.
- **Export** writes a read-only analysis report for the first channel with valid samples. The source CSV log remains unchanged on SD.
- **Quality** examines gaps, missing data, and suspiciously constant recordings. It does not manufacture ECU fault messages.

If no log is loaded, the Analyze workspace opens the saved log browser first. `B` returns along the route used to open a screen, rather than jumping to the old Instruments submenu.

## Map editing and burn controls

**Tune Maps > Open ECU Maps > select a map** displays the actual ECU table on the upper screen. The bottom screen now presents:

- EDIT, - VALUE, + VALUE;
- TOOLS (local edit operations), COMPARE (latest matching local SD snapshot), UNDO, BURN.

**Burn is not a direct instant flash.** It opens the existing confirmation and ECU-side validation/backup/write/burn/readback path, with staged edits required. `B` cancels the confirmation. `B` when leaving a modified map continues to show an explicit unburned-change warning. The Compare button is read-only and reports when there is no compatible snapshot.

## Dashboard

Dashboard's lower screen now exposes four clearly labeled actions: Next Layout, Live / Max / Min, Reset Stats, and Customize. L/R gauge cycling and other existing physical controls remain functional. The upper-screen digital gauges retain the previous HELD/STALE behavior for packet dropouts.

## Important limits

- Main menu and touch action routing are host-source and contract tested, **not console tested**.
- Full devkitARM build, `.cia` installation, touchscreen hit detection, Wi-Fi behavior, ECU burn, and visual polish must be checked on a real Nintendo 3DS.
- The 382-feature development roadmap and CSV tracker are retained unmodified except for noting that the global quick route is now Workspaces rather than MAPS. Previous feature implementation counts are not inflated for navigation changes.

## MSYS2 build

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```
