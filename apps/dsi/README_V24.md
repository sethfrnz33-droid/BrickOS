# Brick Tuner Suite — DSi v24: INI-driven tuning tree

Scope: DSi software only. No 3DS or ESP32 source files are changed by this release.

## Navigation

Home: Tuning Tree, Favorite Settings, Soundboard, Connection. Standalone ECU Maps / Live Graphs / Live Channels launcher tiles were deliberately removed. The INI's **View** menu remains the entry point for live channels and graphs.

Tuning Tree: D-pad to navigate ECU-specified categories and nested dialogs; A opens, B backs out. X on a tuning-tree item or dialog field stars/unstars it. Also use the star on a field's editor; the Favorites workspace opens the same INI target without duplicating its implementation. Favorites are stored under `sd:/BrickECU/dsi-favorites.cfg` or `fat:/BrickECU/dsi-favorites.cfg`, keyed by the full ECU INI signature.

## INI features

- Retains the INI's category/menu/group/dialog/panel/field structure, instead of populating invented tuning sections.
- Maps and curves open through the INI's `[TableEditor]` and `[CurveEditor]` targets; X, Y and Z use actual calibration offsets, page mapping and original scale/translate/limits. Arrays and scalar fields open from `[UserDefined]` dialogs as well.
- Bitfield selectors load predefined labels from raw cached INI lines including `$macro` chains, preserve numerical indexes including `INVALID` holes, and reject an incomplete enum list rather than mislabeling ECU pin choices.
- `[SettingContextHelp]` is read from the original INI on SD, with dedicated on-screen help. Dialog field captions, conditions, and extra INI details are retained and displayed.
- `[UserDefined]` `liveGraph` + `graphLine` are parsed as actual graph definitions. Open a graph from its declaring dialog, view two curves and press X to rotate through the other INI-defined graph lines. B returns to the parent INI dialog.
- `[ConstantsExtensions]` `readOnly` and `requiresPowerCycle` are recognized, as are `xBins` / `yBins` axes marked `readOnly`.
- All parsable `[OutputChannels]` scalar / bit / array definitions remain accessible in the live channel browser behind the INI View menu.
- SD metadata cache schema was versioned to account for the added INI reference structures. v23 snapshots are deliberately regenerated from the original raw INI, then cached so the next start does not re-parse everything.

## Not claimed as completed

TunerStudio command buttons are still displayed, but arbitrary `[ControllerCommands]` operations are **not executed**. Expression-defined runtime fields, dynamic scale expressions, conditions, some built-in TunerStudio dialogs and graphical features are **not yet fully evaluated** on the DSi. The presence of a menu or rendered field is not a claim of working ECU functionality. Bench and device tests remain necessary. Do not write calibrations to a running engine.

## Verification

Host regression tests include HTTP/INI transport, SD caching, calibration read and protected write gates, map axes, enum choices, read-only semantics, parser memory budget and UI routing. The new v24 test covers a 200-choice INI macro list, nested definitions, reserved enum values, curves/tables/axes, INI graph lines, read-only/power-cycle metadata, contextual help. DSi hardware compilation and live-device tuning have not been performed in this environment.

## Build

The DSi/DS build uses devkitPro devkitARM and libnds, libfat and DSWiFi, not the 3DS builder. Example from MSYS2 devkitPro shell:

```
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```

The 3DS application remains separate and unchanged; its existing build command remains:

```
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```
