# DSi INI integration — v24 validation (2026-11-19)

Scope: **apps/dsi only**. Do not modify apps/3ds or firmware/esp32 for the DSi tuning integration.

## Confirmed against uploaded Proteus F7 INI
- Parser valid: yes; truncation: no; OOM: no.
- 1,945 constants; 1,081 live output channels; 195 map and curve definitions.
- 338 menu nodes; 3,500 dialog references; 103 INI live graph definitions; 341 graph lines; 5 pages.
- Host tests pass for 200-choice macro-backed enums with reserved holes, nested macros, contextual help, read-only axes, and protected ECU writes.

## DSi v24 changes staged locally (not yet synchronized to GitHub source)
- Remove redundant ECU Maps and Live Graphs home entries; View remains the INI-driven live visualization path.
- Add SD-backed Favorites to quickly open INI-defined settings.
- Expand INI-backed bitfield dropdown handling to 256 choices.
- Preserve read-only/power-cycle metadata and contextual description text.
- Maintain protected ECU calibration writes.

## Parity acceptance criteria (NOT YET VERIFIED ON HARDWARE)
1. Every INI TableEditor and CurveEditor entry opens and supports accurate axes/values where writable.
2. Every [UserDefined] panel, field, button, and nested panel reflects its INI definition.
3. Every [OutputChannels] reading referenced by View can be displayed and refreshed from live ECU data.
4. All enum definitions, conditional visibility, units/scaling, help text, and related INI semantics are honored, including expressions and special values.
5. No silent fallback to guessed controls, unsupported editing, or truncated option lists.
6. End-to-end DSi hardware tests verify round-trip calibration writes and live data display.

Source ZIP is produced in the current working environment. **Do not regard this document as a source-code deployment.** No 3DS code was modified.
