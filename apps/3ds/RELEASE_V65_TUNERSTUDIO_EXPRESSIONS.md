# Brick Tuner Suite v6.5 — TunerStudio/rusEFI numeric expressions and read-only metadata

This release fixes one major source of incomplete Proteus F7 INI integration. The supplied generated rusEFI file has **116 calibration definitions** whose scale depends on an expression; the old code handled only a small special-case Y-axis conversion.

## Changes

- Keep the original INI declaration for each calibration. Resolve scale, translation, and declared limits together from actual ECU option readbacks.
- Recognize the INI's numeric expressions for metric/imperial, lambda/AFR, fuel-algorithm selection, and VE/ignition/AFR map-load overrides. Nested ternaries in table limit expressions are handled.
- Read six ECU mode flags on the network worker and update calibration definitions only on the UI thread. No assumption of metric mode when the ECU state is unknown.
- Show unresolved conditional axes as unavailable rather than misleading raw bytes. Invalidate derived conversions after confirmed burns and ECU session changes.
- Parse `[ConstantsExtensions] readOnly` records and enforce them for scalar entry, selection lists, curves, map tools, axis edits, direct numeric encoding, and burn requests.
- Keep the previous v6.4 talking mascot, audio, icons, HOME Menu banner, and CAN/HTTP communication unchanged. No DSi files are modified.

## Offline verification

From the repository root:

```bash
python apps/3ds/tests/test_v65_ini_expressions.py
python apps/3ds/tests/test_v65_ini_readonly.py
python apps/3ds/tests/test_v64_mascot.py
```

The numeric test compiles the C expression evaluator with host GCC and processes the real generated Proteus F7 fixture: **116 definitions across 24 independent option combinations**, yielding **2,784 scale/translation/limit cases**. Unknown variables, missing option values and division by zero remain unresolved rather than guessed. Native 3DS compilation and tests on actual ECU/handheld hardware have **not** been verified in this environment.

## Known remaining gaps

This is not yet complete TunerStudio parity. Conditional menu/field visibility, `settingSelector` and `settingOption` presets, other expression families, sophisticated `topicHelp` references, and ECU hardware-validated round-trip editing remain to be implemented and tested. These features must not be marked complete merely because an INI entry was parsed.

## MSYS2 build

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```
