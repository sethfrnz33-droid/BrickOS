# Brick Tuner Suite DSi v34 — INI input/output and field coverage

This release improves generic TunerStudio INI-driven editing using the **uploaded** `rusefi_proteus_f7(1).ini`. It changes `apps/dsi` only. Neither the 3DS application nor ESP32 firmware is modified.

## Source-backed feature work

- Parses **1,976 numeric/text addressable constants** from this full INI (v33: 1,945); 31 more are recovered, including explicit-offset I²C bitfields, shorter ASCII strings, and a missing-comma ETB SENT field reference.
- All **644 distinct bitfield selectors** referenced by the 1,500 existing reachable field references can resolve their full INI choice lists: the SENT input list has **512** indexed slots (many deliberately `INVALID`), so the UI now retains all 512 positions. Reserved options stay hidden and numeric values are never compacted or remapped.
- All Pins Outputs 1/2/3 (24 + 24 + 8 fields) and All Pins Inputs Analog/Digital (14 + 13 fields) resolve as real INI definitions. The standard INI-backed enum editor stages values and uses the existing protected calibration read/write path (writes NOT tested on hardware in this release).
- Adds a **short ASCII-string editor** (e.g., `engineMake`, `vehicleName`, `vinNumber`, and script table names). Touch a letter to overwrite the selected byte; L/R changes character position, Y toggles lower case, X deletes, SELECT enters a space, START requests the usual A-confirmed burn. Writes remain locally staged; back-navigation keeps the existing two-step discard warning.
- Parses the INI's 34 **readoutPanel** sections as read-only live-value groups rather than missing or fake editable controls; gauge-template references are retained. Adds three `settingSelector` labels as noninteractive preset metadata. These multi-field presets are not executed without a verified atomic or safe write protocol.
- Distinguishes selector (`SEL`), text (`TXT`), array (`ARR`) and scalar (`VAL`) types in dialog rows. INI conditional expressions are preserved, including forms where the condition follows an additional numeric option.
- Regenerates the SD metadata snapshot with a new format version, so a stale cached v33 parser image is not incorrectly reused. Existing raw ECU INI is kept.

## Verified on host

- `python3 tests/run_v34_tests.py /path/to/rusefi_proteus_f7.ini`: passes field parsing, 644/644 enum resolution, input/output group references, 512-position options, bit-preserving changes, text edits, readouts, preset metadata, and UI routing. **Does not send ECU writes.**
- Previous v22–v33 UI, map, View, gauge, preferences, transport, and cooperative protected-burn regression suites pass, including mock-server failure cases, after updating stale assertions for the expanded gauge references and new snapshot format.
- The actual `.nds` has **not** been built with devkitARM here, nor tested on physical DSi/ECU hardware. Bench-only validation is required before trusting new controls.

## Explicit limitations: not yet full TunerStudio parity

- Some INI entries are only labels/enumeration definitions without an addressable constant, such as `canReWidebandHwIndex` and `canReWidebandCanId`; no writable ECU address is fabricated.
- TunerStudio **commandButton** operations (including output/actuator tests and automatic calibration) remain visible but disabled: a proper command mapping, safety constraints, and a verified BrickOS transport endpoint are needed before execution. No active-output tests were added.
- `settingOption` selections involve modifying multiple calibration fields. They remain noninteractive until a safe, confirmed multi-field transaction exists.
- Large Lua script blobs (48 KB) remain outside the DSi scalar/text editor's 8 KB RAM buffer; arbitrary conditional visibility and expression semantics still need feature-level tests.
- DSi hardware behavior and comprehensive per-page tuning parity remain unverified.

## Build

```
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```

The package is a complete DSi project directory; it does not claim to contain a newly compiled `.nds`.
