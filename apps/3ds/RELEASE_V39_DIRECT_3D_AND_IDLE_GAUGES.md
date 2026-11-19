# Brick Tuner Suite v3.9 — Direct 3D INI Maps & Unique Idle Gauges

## What changed

1. **Real TunerStudio 2D/3D entry semantics.** The first ID of a `[TableEditor] table = 2DId, 3DId, ...` declaration is a dedicated 2D editor and the second is a dedicated read-only rotatable 3D display. `[UserDefined]` dialogs referring to the 3D alias also open directly in 3D. The shared `zBins` INI field and ECU calibration bytes are used for both. The 3D/2D toggle button and ZR mode switch have been removed; no duplicate entry is manufactured by Brick Tuner Suite.
2. **3D data visibility.** The mesh is built with the exact same `field_value(editor, editor_length, f, y * width + x)` decoder as the 2D editor. Values are annotated at sampled vertices; the bottom of the top LCD displays the selected cell and *actual* ECU Z minimum/maximum. Unexpected table lengths are explicitly rejected rather than filled with synthetic zeroes. A truly flat ECU calibration is labeled as such.
3. **10-second Home fallback.** The **main four-button launcher** now changes its lower screen to an instrument display after 10 seconds of no input. Touching/pressing a control wakes the launcher without accidentally activating an obscured action. The top six home gauges remain unchanged.
4. **No repeated instruments.** The idle lower LCD excludes telemetry channels already displayed on the top LCD: Home excludes the six home gauge slots; Tune excludes the RPM/CLT/AFR reference strip. Matching underlying INI offset/type as well as channel names prevents aliased duplicates. It prefers configured extra gauges and familiar measurement channels, and shows fewer gauges rather than repeating readings or inventing data.

## Controls

- **Tuning tree > 2D VE/table entry:** opens the editable 2D map directly, as before.
- **Tuning tree > VE 3D view or other true 3D alias:** opens the rotatable 3D display directly; Circle Pad rotates, D-pad adjusts view, **B** returns to the tree. The 3D display cannot write to the ECU.
- **Home and non-editing Tune pages:** after 10 seconds, lower LCD displays additional gauges; any control restores the previous menu.
- Staged calibration edits, burn review, and power-cycle prompts are never hidden by the timer.

## Verification

- All eighteen Python regression scripts passed (including new native executable tests of INI alias resolution, real 4×4 nonzero map values, and duplicate gauge filtering).
- Full `main.c` host syntax check passed; existing host-stub warning concerning 3DS `linearAlloc` return type remains unrelated to this change.
- Real devkitARM compilation, hardware rendering, and ECU calibration readback are **not** available in this environment and must be verified on a 3DS.
- No changes to ECU write, backup, burn or bench safety pathways.

The v3.8 guide documents the historical toggle UI; **v3.9 instructions supersede it**.
