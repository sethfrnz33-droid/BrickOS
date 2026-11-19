# Brick Tuner Suite DSi v26 — INI-placed reference gauges & clean settings

This is a complete DSi source update building on v25, with no changes to any 3DS source code.

## Changes

- **Normal settings list:** `West`, `East`, `North`, `South`, `Center` are parsed as INI placement metadata, not fake setting names. Unconditional placed panels are flattened into a single bottom-screen list; conditional panels retain their defined nesting. The visible list is cached on dialog entry to avoid repeated whole-INI scans during screen rendering.
- **Actual reference gauges:** `[GaugeConfigurations]` templates are parsed and resolved from `gauge = ...` entries in `[UserDefined]`, including channel, display name, units and numeric range. Gauge-only panels no longer appear as fake settings: the upper display presents native digital readouts with range bars while the lower screen displays editable fields. L/R pages through gauges; Y opens INI help.
- **Preserved placement:** The original placement is retained in each dialog reference. ECU-defined West/Center settings occupy the bottom list; reference gauges, often placed East/North, remain visible above the list rather than requiring opening unrelated menu entries.
- **Reserved choices hidden:** `INVALID` and reserved INI enum slots are omitted from selection while preserving their original numeric values when an option is chosen.
- **Simple burn confirmation:** START from the calibration editor displays a modal `PRESS A TO BURN / B CANCEL`. A confirms the existing bench-only protected, verified-write transaction; B exits without writing. R stars/un-stars the setting and Favorites remain on SD.
- **INI cache compatibility:** v26 uses snapshot header `BTSMD26` format 4, including gauge templates and placement metadata. The old v25 metadata cache is invalidated; the original INI can be parsed again and a fresh cache generated.
- **Defensive fix:** Virtual read-only curves without an ECU-backed Z field do not dereference a nonexistent constant while loading their descriptions.

## Full Proteus F7 definition audit

- 396 gauge template rows, 67 `[UserDefined]` gauge references, 195 position metadata entries, **0 missing gauge templates**.
- Seven gauge references target calculated runtime channels which can depend on configuration values (for instance, metric/imperial mode). The application must have the needed ECU configuration data to display a computed result; no guessed conversions or false values are substituted.
- Includes the v25 maps/curves, View graphs, live trace, favorites, INI context help and protected calibration writes.

## Validation and limits

- Full INI parser, gauge reference map, SD metadata snapshot save/reload, host compiler, and earlier transport/write/UI regression tests have been exercised.
- **Not tested on actual DSi hardware:** no native devkitARM compiler was available in this environment. Live gauge rendering, ECU-dependent computed channels, and complete TunerStudio parity require hardware testing. The host compile does not produce a `.nds` binary.
- Command buttons from the INI are not implemented as arbitrary ECU commands. They are displayed as unavailable rather than triggered by guesswork.

Build on your configured devkitPro/libnds toolchain from the `DSi` folder with `make`. Use only authorized ECU bench configurations for write verification.
