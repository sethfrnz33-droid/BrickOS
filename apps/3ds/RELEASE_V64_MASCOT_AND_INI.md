# Brick Tuner Suite v6.4 — INI controls and animated talking mascot

## Mascot

- Removes the original background, keeps the source pixel-art character, and emits independent transparent **upper head**, **mouth interior**, and **lower jaw** images.
- The jaw is separated at the **lip opening** (original artwork line y=951 / 1254), not across the nose or forehead. The closed composition reconstructs the exact pixels, verified by `tests/test_v64_mascot.py`.
- Nonverbal, procedurally synthesized 22.05 kHz mono PCM voice. Lip opening is synchronized to the voice's seven syllable envelopes, with mouth fully closing at each syllable boundary.
- On newly confirmed ECU configuration error the mascot briefly appears on the upper screen. The error detail view shows a larger head and repeats the voice when opened. The error text and repair navigation remain readable.
- No physical ECU commands, throttle tests, engine starts, calibration writes, or safety interlocks are modified by mascot code.
- Uses dedicated NDSP channel 2, leaving event and burn clips on channel 0. Respect the existing event-audio preference.

## Two independent Nintendo 3DS HOME assets

1. 48x48 `icon.png`: standard SMDH application tile.
2. `home-banner/mascot_banner.glb`: actual textured 2.5D model, converted to the separate CGFX HOME Menu banner. This replaces the old E46 model. Both are generated from the *same* embedded mascot source.

All source art is reproducibly bundled as `art/mascot/seed.txt`; `python3 tools/generate_mascot.py` produces the PNGs, PCM, and GLB before texture compilation. `make` regenerates the image atlas when art changes. No external audio samples need to be installed on the 3DS SD card.

## TunerStudio INI integration

- INI explanatory text lines in `[UserDefined]` dialogs are now visible as read-only information, instead of silently dropped.
- INI-defined `gauge` controls in dialogs resolve through `[GaugeConfigurations]` to actual output channels; missing telemetry shows `--`.
- Individual calibration controls can be starred from their INI dialog; Y stars the focused item in the tuning tree, and X opens the saved favourites group.
- Removed redundant Live Graphs and Live Channels shortcuts from Logging, retaining INI View and existing instrument surfaces.

**Not full TunerStudio parity yet:** `settingSelector` presets, complete conditional expression evaluation, nested panel/layout semantics, generic INI commandButton dispatch, and other extended dialogs are still incomplete. Physical test commands remain gated/unavailable unless separately validated.

## Tests and build

- `python3 tests/test_v64_mascot.py`: **PASS** on host, including a genuine C syntax pass against the provided 3DS stubs, exact closed-face pixel reconstruction, audio assets, and structurally valid GLB.
- Both shell scripts passed `bash -n`; GLB loaded with `trimesh` in the host environment.
- The full devkitPro `.3dsx`/`.cia` build, actual CGFX converter output and original 3DS rendering/voice playback **have not been verified** on hardware. The legacy v3x-v6x screenshots/build tests still target the previous E46 banner and may need updating.

Windows MSYS2 devkitPro command:

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```
