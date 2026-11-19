# DSi v2.2: 3DS-style workspace, bitfield editing and SD audio diagnostics

**Release scope:** v2.2 complete DSi-only source ZIP distributed to the user as `Brick-Tuner-Suite-DSi-v22-3DS-Style-UI-Audio-Editing.zip`. **The ZIP itself is not committed here.** No 3DS source or BrickOS ESP32 firmware is modified by this documentation change.

## DSi-specific changes

- Replaced six small home tiles with the **original 3DS icon-left vertical workspace list**, consistent 9×14 bitmap label glyphs and D-pad/touch selection.
- **L/R on HOME cycles 2, 3, or 4 top-screen gauges**. Default 2 gauges are AFR/VBAT; extras are actual INI-defined live channels. User selection persists in `BrickECU/dsi-layout.cfg` on the DSi SD card. Six lower-screen gauges after ten seconds idle are retained.
- Expanded ECU field editor to support **INI-defined bits/enum values** in addition to maps, curves, scalars and arrays. Bit writes are locally staged; the encoder preserves unrelated bits of the backing ECU value. Supports directly defined labels and some `#define`-referenced options, and local maps up to 2048 cells. Fixed nested-dialog/map screen overlap.
- Added actionable audio diagnostics and ARM9 cache flush ahead of DSi hardware playback request, with 14 converted PCM8 files. Audio files **must** be installed on the DSi SD card at `BrickECU/sfx/00.pcm8` through `BrickECU/sfx/13.pcm8`; they are not embedded in the `.nds` ROM. Restart after installing assets.
- Retained v2.0+ persistent HTTP connection and cached INI, v2.1 30 Hz **target** and safe confirmation/backup/readback write flow. Excludes logging and log analysis.

## Testing and limitations

All 13 local host regression scripts pass, and C source passes GCC `-Wall -Wextra -Werror -fsyntax-only` under DS API stubs. Native devkitPro ARM9 compilation, audible sound output, real-world throughput and ECU writing on DSi are unverified. Some complex rusEFI INI widgets remain unported; do not claim complete 3DS feature parity.

## Merge safety

The ZIP contains only `BrickECU-3DS-Probe/DSi/`, not the complete historical 3DS project. Add it under `apps/dsi/` in BrickOS when doing a source import, **without overwriting current `apps/3ds/`, ESP32 firmware or shared root build scripts**. The DSi local build command is:

```sh
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```

The 3DS build remains `cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh`.
