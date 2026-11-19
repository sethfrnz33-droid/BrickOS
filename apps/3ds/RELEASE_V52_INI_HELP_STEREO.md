# Brick Tuner Suite v5.2 — INI Help / Stereo Alignment

## Tuning subfolders

- The bottom screen in **opened group folders** (`PAGE_GROUP`) and **INI dialog field lists** (`PAGE_DETAIL`) now displays RPM, CLT, and AFR in three compact live gauge cards, the selected setting label, and the setting's actual explanation from the ECU's `[SettingContextHelp]` section where present.
- `SettingsContextHelp` is not a substitute for ECU data: no generic descriptions are invented. If the INI has no explanation for a field, the UI says so.
- Help is resolved by the exact submenu target, by the table's actual Z calibration field, or by referenced dialog fields. Quoted help text is decoded as plain text; simple HTML breaks/entities are converted for 3DS display. The descriptions are bounded and word-wrapped.
- Root Tune categories and unopened top-level folders remain unchanged and do not show help paragraphs. A/B navigation remains intact, and tapping help cannot accidentally trigger an edit.

## 3D rendering

- Corrected the map-eye displacement so nearer calibration surfaces use the same crossed-eye direction as the frontmost menu items.
- Reduced excessive map-eye separation from 11.5px per-eye coefficient to 3.2px, tied to the 3D slider, to reduce ghosting and difficult binocular fusion.
- Selected menu cards, icons and text now move together on the same parallax plane; previously the box could shift by 7–9px while its label moved just 0.8px.
- Reduced main dashboard and Tune gauge stereo parallax to more moderate values, preserving foreground dashboards and recessed Tune gauge rail.

## Verification and limitations

- 30 Python regression suites passed, including v5.2 native tests of source help parsing, phrase decoding, field lookup, stereo signs and source integration.
- Legacy tests updated to check the corrected, moderate parallax rather than previously excessive disparities.
- Native write protocol mock passed success, backup failure, RPM-running, baseline conflict, write rejection and burn rejection cases; source write/burn path unchanged.
- Host GCC syntax check passed with the expected 3DS stub pointer-cast warning; devkitARM device compilation, eye separation and readability on a real Nintendo 3DS remain unverified here.
- Some TunerStudio `topicHelp` entries refer to external help pages or local project files, which are NOT embedded descriptions and therefore are not displayed as in-INI help.
