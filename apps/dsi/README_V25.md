# Brick Tuner Suite — DSi v25: INI editor and display fixes

Only DSi source changes. No `apps/3ds` or ESP firmware files were modified.

## Visible UI fixes

- **START no longer terminates the application.** When editing a setting, START toggles its Favorite. Protected calibration burn remains on **R twice**; the existing ECU signature verification, backup and readback safety checks are retained.
- Removed duplicate folder-name “settings” arising from INI `dialog` titles and topic-help metadata. Folder headings appear as headings, not selectable fields.
- Maps now use a heat-color matrix, selected-cell outline, dynamic X/Y axis labels and values, trace indication, and compact labels on the upper screen. Editor tools and INI descriptions occupy the lower screen.
- Tuning/dialog lists display eight rows on the lower screen with measured-height controls and compact, consistent typography; graph lists paginate in eight rows instead of clipping or overlapping the footer.
- `[SettingContextHelp]` and `[UserDefined]` topic help are displayed on upper-screen setting/dialog views and in the full-page help overlay. These are sourced from the INI rather than generated prose. When the INI provides no help, that is shown explicitly.
- All `[CurveEditor]` `yBins` series are separate selectable curves (27 additional series in the provided Proteus F7 definition).

## INI axes and live data

- Unquoted nested expressions with commas are preserved during CSV parsing.
- ECU-controlled format expressions are evaluated with strict variable lookup of the actual calibration values for metric/lambda settings, fuel algorithm and load-override modes. The app reads these ECU bytes before decoding conditional axis scales; unsupported expressions remain read-only, never silently guessed.
- TunerStudio virtual arrays from `[PcVariables]` / `[ConstantsExtensions]` `defaultValue` (for gear/range/solenoid axes and warmup display) are shown as **read-only INI-defined values**, not read from fabricated ECU offsets. Explicit defaults can initialize a read-only curve; virtual arrays without defaults remain unsupported.
- Trace plotting resolves raw and simple computed `[OutputChannels]` expressions, and supports virtual axes. The Live request size includes runtime dependencies for visible graph lines.
- The View graph lower screen shows INI-declared graph lines with their live values, including evaluated simple computed channels, paginated in groups of eight; the upper screen shows the selected live graph.

## Verified

Host GCC build of `source/main.c` passes `-Wall -Wextra -Werror`; parser regression tests run against the full uploaded Proteus F7 INI (222 map/curve series, 1,081 channels, 338 menu nodes) with no truncation, and host transport/safety/regression tests pass. DSi hardware, devkitARM `.nds` build, map trace at the ECU, and every INI-defined TunerStudio control have not been validated here. Some TunerStudio-only functions (notably command buttons and computed analyzer values with no declared defaults) are not ECU memory-backed and are deliberately not pretended to work.

## Test

```sh
python tests/run_v25_tests.py /path/to/rusefi_proteus_f7.ini
```

## Build (MSYS2 devkitPro shell)

```sh
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```

No changes are required to the 3DS build.
