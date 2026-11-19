# Brick Tuner Suite v3.5 — Two-screen tuner workflow

## Design references (verified November 2026)

The TunerStudio MS Lite Reference Guide documents function-oriented navigation: Basic/Load, Fuel, Ignition, Startup/Idle, Acceleration Enrichment, Boost/VVT, 3D maps, and CAN/test modes. Haltech NSP documents its visible navigation tree and direct keyboard cell entry. Brick Tuner Suite adapts those workflow patterns to a 400x240 upper screen, 320x240 lower touch screen, and New 3DS C-stick.

- TunerStudio official reference: https://www.msextra.com/doc/pdf/Megasquirt3_TunerStudio_MS_Lite_Reference-1.5.pdf
- Haltech NSP navigation: https://support.haltech.com/portal/en/kb/articles/overview
- Haltech NSP table editing: https://support.haltech.com/portal/en/kb/articles/table-data-manipulation

This is **TunerStudio-style**, not TunerStudio's proprietary UI, and not an exact per-firmware replication. No unsupported ECU configuration pages were fabricated. Sections come from the ECU-provided INI and are categorized by their actual names; unfamiliar sections remain in **Other ECU Pages**.

## What changed

1. **Tune** enters a dedicated ten-category tree (Basic/Load; Fuel; Ignition; Startup/Idle; Accel Enrich; Boost/VVT; 3D Maps; Advanced/I-O; CAN/Tests; Other ECU Pages). Selecting one opens only real sections from the active INI. All ECU Calibration Maps appears explicitly under the maps category when tables exist.
2. **Upper screen** follows the highlighted lower-screen category, section, setting, or table. It displays up to nine matching child entries plus live RPM, coolant and AFR reference gauges down the left side, with visibly held/stale data.
3. **Lower screen** displays seven category/section entries or six dense ECU-setting/table rows; settings remain navigable with D-pad, touch, shoulders, and C-stick.
4. **Maps and curves** support 0.001, 0.01, 0.1, 1, 10, 100 step sizes, selected with L/R or C-stick left/right. In locally staged edit mode, A opens the 3DS keyboard prefilled with the selected value for exact decimal entry. Parsing validates the INI's range and numeric representation; no numeric input bypasses existing burn checks. C-stick up/down jumps multiple map or curve cells. Curve editor shows five adjacent X/Y points and allows focus selection for an editable X breakpoint versus the Y output.
5. **ECU fault UI** leaves a compact top screen status strip during ordinary tuning instead of covering the setting you're editing. **SELECT+Y** opens related fault-repair actions while standalone **Y** still increases a value. The repair screen links to matching INI fields and always provides browsing and error-refresh fallbacks. No claim is made that all error messages can be mapped automatically.
6. **Required ECU power cycle** after a verified relevant burn has its own large, persistent overlay on both screens. Press **A** to acknowledge; the software requests new ECU status afterwards but does **not** treat the key press as proof a power cycle happened.
7. **Build isolation** from v3.4.1 is retained: the Makefile compiles `main.o` and `font_assets.o` explicitly so demo assets do not create duplicate font symbols during an in-place upgrade.

## Navigation flow

Home -> Tune -> Functional category -> ECU section -> ECU setting or curve/map -> locally stage changes -> guarded burn review.

Home has independent Dashboard, Logs, Analyze and Settings/Diagnostics workspaces. Upper screen context preview appears while browsing ECU tuning sections, menus and maps; the map/curve itself is displayed on the upper screen when editing. Dashboard mode remains separate.

## ECU safety, build and test status

- The existing staged write/burn flow, RPM restriction, ECU-session verification, backup step and post-burn verification remain in place. Input on the keyboard does **not** immediately write to the ECU.
- The updated source passed host syntax checks and source-level workflow tests. Physical display output, software keyboard behavior, ECU-specific INI labels and the 3DS/2DS compiled app were **not** verified in this environment.
- Test on the 3DS while connected to a bench ECU, with outputs safely disconnected, before using an installed ECU.

### MSYS2 build command

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

Extract the ZIP preserving its `BrickECU-3DS-Probe/` outer project folder. Replace/update the corresponding original project folder before running the build command.
