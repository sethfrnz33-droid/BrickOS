# Brick Tuner Suite v3.5 — Controls

| Context | Input | Action |
|:--|:--|:--|
| Home | Tune | Open TunerStudio-style category tree |
| Category tree | D-pad up/down | Move one category |
| Category tree | C-stick up/down | Jump three categories |
| Category tree | A / B | Open category / back to Home |
| Section list | D-pad up/down | Move one actual INI section |
| Section list | L/R or C-stick left/right | Previous/next tuning category |
| Section list | C-stick up/down | Jump five sections |
| Section list | A / B | Open / back |
| ECU menus | D-pad | Select settings; upper screen previews context |
| ECU menus | Touch | Tap row; tap same row again to open (menus); maps open directly |
| Map/curve | SELECT | Toggle local edit mode after ECU data loads |
| Map/curve | D-pad | Move current cell; on curves left/right selects point and up/down selects X/Y focus |
| Map/curve | L/R or C-stick left/right | Change step size (0.001 to 100) |
| Map/curve | C-stick up/down | Jump five cells/curve points |
| Map/curve | X/Y | Decrease/increase selected value by chosen step |
| Map/curve | A in staged mode | Exact decimal entry using 3DS keyboard; value is local until burn |
| Map/curve | START | Review guarded ECU burn when changes are staged |
| Fault warning | SELECT+Y | Open actionable ECU error screen (Y alone remains value increase) |
| Fault page | A / B | Follow a linked setting or recovery action / back |
| Power-cycle prompt | A | Acknowledge persistent prompt; request fresh ECU status (not automatic detection) |

## Layout at a glance

**Upper LCD during Tune navigation:** 3 narrow live RPM / coolant / AFR gauges on the left, and a detailed list of the currently highlighted function's real ECU settings on the right.

**Lower LCD during Tune navigation:** visible categories or settings with more rows at once; touch and buttons operate on the same highlighted item.

**During map/curve editing:** upper LCD is the actual chart or map; lower LCD presents precision, X/Y point values and editing controls. The 3DS keyboard appears only for exact numeric entry.

## Limits

The grouping is based on ECU-provided INI section names, not a hardcoded list of fictitious settings; uncommon headings are accessible under **Other ECU Pages**. The original 3DS has no New 3DS C-stick, so use its D-pad and shoulder buttons. Values are rounded to the ECU field's actual numeric resolution after validation, so some decimals may be unrepresentable.
