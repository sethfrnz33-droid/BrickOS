# Brick Tuner Suite v3.7 — TunerStudio Two-Screen Tree

## Main change
The v3.6 upper screen repeated the category list and children side by side. v3.7 instead implements the requested focus-transfer interaction:

| Step | Upper screen (400x240) | Lower touch screen (320x240) |
| --- | --- | --- |
| Home | Six actual ECU gauges + Wi-Fi/battery/live RX Hz rail | Tune / Analyze / Logging / DS Settings |
| Enter Tune | ONLY the ten TunerStudio category headings (Setup, Fuel, Ignition, Cranking, Idle, Advanced, Sensors, CAN-bus, Controller, View), with the user-supplied pictograms | Actual `Menu` entries for the highlighted category, in INI order, immediately previewed |
| Highlight Fuel (no A required) | Fuel category selected | VE, injector setup, fuel correction, etc., **only where the connected ECU INI actually defines them** |
| Press A while focused on upper categories | Top category remains selected | Cursor transfers to lower preview. **No extra category page.** |
| Press A on lower folder | Top category list remains visible | Folder children replace preview list. B returns to parent list |
| Press A on lower ECU item | Editor or INI dialog is shown on top; maps and curves use their dedicated top visualization, scalar value has a dedicated top panel | Value editing/burn controls, dialog actions, or list navigation; existing guarded write flow |
| Press B from editor | Return to the top category tree | Last-previewed category and lower cursor retained |
| Press B from lower list | Top category tree takes cursor focus | Remains a live preview for the highlighted category |
| Press B while category tree focused | Home dashboard | Home four-mode launcher |

There is **no standalone "All ECU Maps" navigation route**: a table or curve appears only where an INI `subMenu` or `groupChildMenu` points to it. `Other ECU Pages` appears only if the ECU definition has actual unmatched menu entries. This preserves incompatible/future INI categories instead of hiding them.

### Graphics
- 10 category pictograms were extracted from the user's provided TunerStudio tab screenshot (`art/reference/tuner_studio_tabs_source.png`).
- Three row icons (blue folder, colour-table, configuration dialog) came from the user's supplied TunerStudio Ignition-menu screenshot; they are embedded as transparent sprites under `art/tsicons/*.png`.
- `tex3ds` builds the icon atlas as part of `make`; `source/ts_icons.S` embeds it, and `main.c` loads it with `C2D_SpriteSheetLoadFromMem`.
- Source manifest is explicit: `main.o font_assets.o ts_icons.o`, preventing duplicate link objects left by unrelated demo projects.

### DS Settings: strictly console-specific
The DS Settings workspace now contains **Display & Gauges**, **Wi-Fi & ECU Connection**, and **Menu Sound On / Off**. Display & Gauges holds the layout and nine configurable gauge slots; its former redundant links to logging and diagnostics were removed. Logging and analysis stay in their dedicated home modes, ECU tuning stays in the TunerStudio tree.

### Input controls
- Top category focus: D-pad/Circle Pad up/down switches rows, left/right switches columns; C-stick or L/R also navigates category selection.
- A on top: focus the lower list, *without opening* the currently highlighted category into a redundant page.
- Lower focus: D-pad/Circle Pad up/down changes item; C-stick up/down or L/R moves five items; A opens selected item/folder, B returns toward top.
- Lower touch: first tap positions cursor/focus, second tap of selected row opens it.
- B from top returns home; SELECT from tune tree returns home immediately.
- Inside map/curve editor: existing v3.5 decimal entry, adjustable step sizes, guarded burn, fault behavior and power-cycle acknowledgement are preserved.

### Build under MSYS2

```sh
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

### Testing and limitations
16 Python host regression scripts pass, including a compiled test that invokes the real traversal/focus functions with a synthetic representative INI hierarchy, validates INI order/nested folders, and ensures unreferenced tables never create a global map browser. A full `main.c` host syntax check also passes.

**Not validated here:** `tex3ds`/devkitARM compilation, actual 3DS graphics, button behavior on hardware, ECU connection, and write/burn on a real ECU. Host tests do not prove any of those things. Calibration writes still require the existing safety checks and deliberate user confirmation; no ECU protocol changes were made in this release.
