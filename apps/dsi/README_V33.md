# Brick Tuner Suite DSi v33 — Clutch LEGO Sound Cycle

This DSi-only update adds the seven **user-supplied** LEGO sound effects and plays one on each confirmed change of the clutch switch. The sounds rotate in the same order as the files below and wrap around after the seventh cue; both pressed and released edges count.

## What triggers it

- The default source is the actual rusEFI/Proteus F7 **`[OutputChannels] clutchDownState`** live bit, not `clutchDownPin` (which is only the input-pin setting).
- If your wiring uses the separate clutch-up switch instead, create `/BrickECU/clutch-source.cfg` on the DSi SD card containing a single line: `up`. This selects `clutchUpState` on the next app start; remove the file or use `down` to select the default.
- The client requests enough live ECU bytes to receive that channel on any tuning page, not just a View page.
- The first good sample is only a baseline. A changed state must persist for **two distinct ECU samples** before playing; duplicates and brief spikes do nothing. Missing live data clears the baseline, so reconnect does not trigger a sound.
- The sounds are UI-only effects. No ECU calibration data, outputs, or firmware are changed.

## Install SD audio

Copy the entire `SD_Files/BrickECU/` directory in this release onto the SD card's `/BrickECU/` folder, retaining the original `sfx/` assets and the v32 Kevin RPM assets. The clutch cues live at:

| File | Original upload |
|---|---|
| `clutch/00.pcm8` | LEGO Star Wars generic rebel grunt |
| `clutch/01.pcm8` | LEGO Star Wars rebel trooper death |
| `clutch/02.pcm8` | LEGO Star Wars minikit collect |
| `clutch/03.pcm8` | LEGO Star Wars transition |
| `clutch/04.pcm8` | LEGO Yoda death |
| `clutch/05.pcm8` | LEGO Star Wars droid death |
| `clutch/06.pcm8` | LEGO Star Wars IG-88 death |

Converted to signed 8-bit mono PCM at 11,025 Hz for libnds playback. A missing file does not disable tuning; the app notes the missing cue.

## Validation

Host tests verify the actual Proteus F7 INI field offset and bits, both transition directions, debounce, repeated-sample suppression, no false alerts after reconnect, seven-cue ordering, asset integrity, and playback calls through stubbed libnds audio functions. Existing DSi UI, INI, and nonblocking burn regressions pass. **The `.nds` was not built with devkitARM in this environment, and live DSi/ECU hardware operation has not been verified.**

Only `apps/dsi` is changed; `apps/3ds` and ESP32 firmware are intentionally untouched.
