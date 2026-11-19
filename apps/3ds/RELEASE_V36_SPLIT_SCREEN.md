# Brick Tuner Suite v3.6 — True split-screen tuning

This update replaces the redundant dual-screen lists with a distinct information architecture based on the supplied rusEFI/TunerStudio screenshots.

## Home workspace

- Upper 400x240 screen: six live ECU gauges in a fixed two-column, three-row grid. Gauge slots 1–6 retain their saved INI-selected channels; no readings are synthesized.
- Slim left-side status rail: Wi-Fi state, the 3DS battery level from PTMU (ordinal 0–5, **not** a percentage), and the measured live telemetry receive rate in Hz. An unavailable battery level and unavailable ECU rate are marked as such.
- Lower 320x240 touchscreen: four large buttons in a 2x2 grid — **Tune, Analyze, Logging, DS Settings**. The D-pad / Circle Pad and A also navigate them. There is no separate home Dashboard button that wastes a slot.

## Tuning workspace

- Upper screen owns the INI-derived tree. The left column has the ten categories in the supplied screenshots: **Setup, Fuel, Ignition, Cranking, Idle, Advanced, Sensors, CAN-bus, Controller, View**.
- **Other ECU Pages** is the additional visible fallback for firmware-provided categories outside those ten. No INI menu item is discarded just because its title is unfamiliar.
- As the user highlights one of the categories, the upper right previews its **actual INI-defined settings**, not another copy of the category name. A category with a single ECU menu opens its actual settings directly on A; if multiple sections match, it shows the section picker first.
- Diving into any category, subfolder, dialog, or map browser keeps the upper screen dedicated to the live selectable tree. The lower screen switches to context, actions, and three reference readings (RPM, CLT, AFR).
- No fake options or ECU data. Unavailable entries show that the category has no matching section.
- Map and curve editors remain their full-screen specialized editors, with existing direct decimals, guarded write/burn flow, power-cycle acknowledgement, and fault navigation intact.

## Controls

| Screen | Control | Action |
|---|---|---|
| Home | Tap one of four tiles | Open that mode |
| Home | D-pad / Circle Pad, A | Choose mode, then open it |
| Tune top tree | D-pad Up / Down | Select a category, section, or INI item |
| Tune top tree | A | Open selected category / item |
| Tune top tree | B | Go back without writing anything |
| Tune top tree | Touch lower `A OPEN SELECTED` area | Open the highlighted top-screen item |
| Tune top tree | X or lower `X ALL ECU MAPS` area | Browse all true INI-defined maps/curves |
| Tune top tree | SELECT | Return to the four-mode home screen (except inside the existing editor) |
| Tune category menus | L/R | Move between nonempty TunerStudio tabs |
| Value/map editor | Existing v3.5 controls | Exact decimal entry and guarded bench-only write unchanged |

## Build

Unzip as a **project folder** into Downloads, merging it with the existing `BrickECU-3DS-Probe` folder. Then in MSYS2 run:

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

The Makefile continues to compile only `main.o` and `font_assets.o` so unrelated game projects cannot duplicate linker symbols.

## Verification limitations

- All **15 host-side Python test scripts passed**, including the new v3.6 split-screen contracts, compiled navigation classifier, and a syntax-only compile of the complete `main.c` with local 3DS host stubs.
- **Not verified on a 3DS or against a real ECU.** Host tests do not establish console appearance, real PTMU responses, Wi-Fi reliability, frame rate, or successful hardware builds. No altered ECU write protocol is introduced in this release.
- libctru API declarations verified against devkitPro's `libctru/include/3ds/services/ptmu.h` (`ptmuInit`, `ptmuExit`, `PTMU_GetBatteryLevel`, `PTMU_GetBatteryChargeState`).
