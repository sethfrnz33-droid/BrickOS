# Brick Tuner Suite DSi v29 — fixed curve links, Favorites, status controls, and 3D mesh

Scope: `DSi/` only. Neither the 3DS app nor BrickOS ESP32 firmware is modified.

- **3D tables:** mesh vertices are spaced by integer table-cell indices, avoiding disproportionately stretched cells caused by irregular X/Y bin values. Actual INI X/Y values remain in tuning and trace data.
- **2D curves:** `CurveEditor` panels embedded via `panel = curveId` now open the matching curve instead of being treated as empty folders; a 1D curve gets a dedicated full-width X/Z plot with proper values, bins and selected point.
- **Favorites:** opening a favorite runs through the full Tuning dialog navigation path; B returns to Favorites. The original target and label are preserved.
- **INI semantic controls:** literal `text` and static `field` annotations are muted, noninteractive labels; command buttons get a distinct disabled `CMD` visual and are **not** misrepresented as editable settings. Unsupported commands remain disabled; no new ECU commands were added.
- **Live indicators:** `[UserDefined] indicatorPanel` and `indicator` definitions now produce read-only ON/OFF/UNKNOWN status rows based on actual ECU live channels where available, with label variants derived from the INI. Missing live values are shown as UNKNOWN, never guessed OFF.
- **No firmware or protected-burn changes.**

## Validation

From `DSi/`:

```bash
python3 tests/run_v29_tests.py /path/to/rusefi_proteus_f7.ini
python3 tests/run_v28_tests.py /path/to/rusefi_proteus_f7.ini
python3 tests/run_v27_write_tests.py
```

Host regressions do not replace hardware verification of frame rate and navigation.
