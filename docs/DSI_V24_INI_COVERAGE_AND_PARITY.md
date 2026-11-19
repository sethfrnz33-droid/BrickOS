# DSi INI integration — v25 status and parity gates (2026-11-19)

Scope: **apps/dsi only**. Do not modify `apps/3ds` or `firmware/esp32` as part of the DSi editor project.

## Deployment
- **v25 DSi source is now committed to main:** [cb4f92aa](https://github.com/sethfrnz33-droid/BrickOS/commit/cb4f92aa91ca744db451a9089e0e661a1ae879f0).
- The commit changes 10 files, all under `apps/dsi`. No 3DS or ESP32 files were changed in this commit.
- Full DSi release notes: [apps/dsi/README_V25.md](../apps/dsi/README_V25.md). Keep this document synchronized with future DSi releases.

## INI coverage / host-verified
- Full Proteus F7 definition parsed without reported truncation: 1,945 constants; 1,081 live output channels; 195 map/curve definitions with **222 series** after preserving repeated curve yBins; 338 menu nodes; 103 graph definitions; 5 config pages.
- 200-entry macro-backed enums, reserved holes, nested macros, contextual help, virtual axes with defaultValue, simple computed channels, and safety checks have regression coverage.
- V25 includes colored maps with X/Y labels, removal of fake folder title fields, denser lower-screen layout, paginated View graph lines, INI help, and START-as-Favorite instead of exit. Calibration burn remains R twice.

## Not established (do not claim 3DS or TunerStudio parity yet)
1. Every INI TableEditor / CurveEditor entry displays correct axes, units, colors and writable values on real DSi hardware.
2. All [UserDefined] panels, conditional controls, buttons, nested dialogs and enum choices obey the definition, including expressions not yet supported.
3. All [OutputChannels] signals referenced by View have verified live transport updates and correct numerical decoding.
4. Trace positions follow actual live X/Y channels on the associated maps/curves, including complex expressions.
5. Complex derived expressions, virtual fields without explicit defaults, and TunerStudio-only commands are handled appropriately without pretending to modify ECU memory.
6. End-to-end .nds build plus DSi hardware tests cover map editing, burn/readback, live View, offline cache and navigation.

**Host tests do not demonstrate hardware correctness or full TunerStudio feature parity.**
