# Brick Tuner Suite DSi v28 — bigger menus, faster nested dialogs, true 3D tables

**Scope:** `DSi/` only. This release does not modify the Nintendo 3DS application or BrickOS ESP32 firmware.

## Navigation readability

- Tuning folders, nested setting-panel lists and top-screen category tree now use the native **9×14 font** (previously 6×9 for the main navigation items).
- Eight selectable rows remain visible. Labels clip with ellipses instead of overlapping Favorites/icons or the bottom footer.
- A dialog-owner hash index resolves INI panel children, gauge-only subpanels, and panel entry lists without repeatedly scanning all 2,799 references. The original INI order is preserved.
- INI `[SettingContextHelp]` text is fetched by an on-SD **hashed offset index** rather than rescanning the whole INI whenever the selection changes. Hash matches are checked against the full source key before showing text. The index is reset after an ECU INI refresh.
- Previously retrieved computed reference-gauge formulas are cached during the session, avoiding repeated file scans when revisiting the same nested panel.

## Actual three-dimensional tuning tables

- A two-axis table **opens in a real 3D mesh view by default**. Rows and columns are connected into elevated shaded surface triangles using the real ECU Z values and the INI's X/Y bins when present.
- Shaded surfaces have a full blue/green/yellow/red height scale, grid lines, occlusion/depth testing, and different markers for current tuning selection and live INI trace.
- Tap **2D MAP / 3D MAP** at the upper right of the lower-screen editor to toggle between 3D surface and the existing numeric heatmap, without changing the tune or losing cursor position.
- In 3D mode, hold **L + D-pad LEFT/RIGHT** to rotate, **L + D-pad UP/DOWN** to change the camera elevation. D-pad alone navigates exact table cells; SELECT cycles the existing Z/X/Y editing focus; X/Y adjust the focused value; A opens the numeric keypad.
- START then A confirmation retains the v27.1 nonblocking, protected burn path; R still favorites the current tuning setting, B returns. On-screen STEP control remains available in 3D mode.
- Actual mesh rendering is done in **software on the DSi framebuffer**; this is not the 3DS Citro3D renderer. Large tables are displayed with up to 17×17 sampled vertices for rendering speed; **all original tuning cells remain editable** at their full resolution.

## Validation

Run from the `DSi/` folder:

```bash
python3 tests/run_v28_tests.py /path/to/rusefi_proteus_f7.ini
python3 tests/run_v27_write_tests.py
python3 tests/run_v26_tests.py /path/to/rusefi_proteus_f7.ini
```

The v28 host tests cover real-INI owner-index content and original order, native larger glyph raster, complete INI contextual help, a colored 3D heightfield with yaw/elevation variation, out-of-bounds guards, and large-table decimation. Existing protected-write/transport regressions must also pass.

**Not hardware verified:** The `.nds` has not been compiled with devkitARM/libnds or profiled on physical DSi hardware in this environment; measured hardware frame rate and end-to-end 3D/editor/burn behavior must be checked on a DSi. The software mesh is a viewer on top of existing table edits, not new ECU commands.

## MSYS2 build

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```
