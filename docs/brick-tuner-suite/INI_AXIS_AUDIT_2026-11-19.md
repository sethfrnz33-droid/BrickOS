# Brick Tuner Suite v6.4 — INI parsing and axis audit (handoff)

**Status:** Changes are prepared and host-tested locally, **not merged to BrickOS main**. This document was written on an isolated branch to avoid interfering with another active agent.

## Bugs reproduced in v6.3

1. Multiple TunerStudio `[TableEditor]` entries can share `zBins` but use different `xBins` and `yBins`. The previous editor picked the first table with a matching Z name, giving the opened map the wrong breakpoint arrays.
2. The 2D map footer printed `RPM > LOAD ^` regardless of the actual INI fields. Some maps are TPS, coolant, pressure, time, and other axes.
3. When an axis had a conditional scaling expression that had not yet been resolved from the ECU, the previous decoder could print the raw integer as though it were scaled physical units.
4. Axis array lengths were not checked against the selected Z table's X/Y dimensions, allowing missing or misaligned breakpoints.

## v6.4 local fixes

- A selected table's exact identity (`table id` / `alt` alias) is carried into the editor, including search, direct menu actions, trace and detail navigation. The 2D and 3D aliases still share the same actual Z bytes.
- Decode explicit `xyLabels` for tables and `columnLabel` for curves; fall back to the named INI channel or field when labels are unavailable or are unevaluated expressions. Labels are **not** universally RPM/Load.
- Validate the axis array count against the respective map dimension and show a mismatch instead of reading the wrong offset.
- Display unknown axis scale as `--` with a clear status; never silently present unscaled raw values as physical units.
- Retain the existing ECU-based scale resolving code and all existing read, write, and burn safeguards.

## Verification

Native regression `test_v64_ini_axes.py` compiled the production C source using host stubs and parsed a synthetic INI with **two tables sharing the same Z field**. It verified the selected X/Y field identities, map axis labels, curve column labels, mismatched axis dimensions, and unknown-scale handling. Host syntax check passed. The actual devkitARM build and real ECU validation are still outstanding.

## Known gaps — important

This does **not** mean the entire TunerStudio INI language is supported. Outstanding work includes:
- General `{ ... }` expressions, dynamic axis labels, and arbitrary `@@...@@` preprocessor macros
- Variable-size arrays such as `[{FUEL_TABLE_COLS}x{FUEL_TABLE_ROWS}]`
- Multi-series `[CurveEditor]` declarations with multiple `yBins`
- Comparison against actual generated rusEFI INIs and live ECU calibration reads

Do not silently guess units, dimensions, or axis values. Unresolved data must be marked unavailable, not substituted with fabricated values.

## Coordination

**Do not overwrite an active agent's changes or auto-merge this branch.** Apply and test these changes against the then-current Brick Tuner Suite source, reconcile concurrent edits, and confirm the actual hardware before integration.

References: [rusEFI TunerStudio INI](https://github.com/rusefi/rusefi/blob/master/firmware/tunerstudio/tunerstudio.template.ini) and TunerStudio ECU-definition documentation.
