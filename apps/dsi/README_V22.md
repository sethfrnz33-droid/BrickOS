# Brick Tuner Suite DSi v2.2 — UI, editable fields, audio

This release updates the standalone DSi source folder only. The running BrickOS ESP32 firmware and actively developed 3DS edition are **not** included or changed. This version retains the working v2.1 HTTP keep-alive transport, SD-cached ECU INI, ECU-defined channel maps, and 30 Hz dashboard **target**.

## Changes

- **L / R on HOME** cycles **2 / 3 / 4 upper-screen gauges**. Two-gauge mode shows large AFR/VBAT readouts by default; 3 and 4 gauges use additional real INI-defined channels. Layout choice persists in `BrickECU/dsi-layout.cfg` on the DSi SD card. The two primary gauge assignments persist separately.
- The lower-screen HOME page is now a **single vertical, icon-left workspace list** with original 3DS-derived category icons, an obvious highlight, and D-pad/touch/A navigation. Duplicate bottom navigation is gone.
- Labels use a uniform **9×14 fixed-baseline bitmap font**, rather than mixing multiple tiny sizes. The upper gauges use enlarged digital values. The 10-second lower-screen idle-gauge mode is retained, with duplicate channel suppression.
- Open INI-defined **bitfields** with enumerated choices rather than rejecting all as uneditable. Numeric entry updates the selected bits while preserving unrelated bits in the same ECU storage word. Bitfield choices support directly defined labels and basic `#define`-referenced label lists. Scalar, array, and map/curve editing paths remain available. Larger maps up to 2048 cells are supported in the local editor.
- A nested INI dialog no longer draws on top of an opened map or setting editor; the active edit screen takes priority.
- On a sound cue the ARM9 now flushes the audio sample cache before sending it to the DS sound engine. The Soundboard reports missing SD assets and basic playback problems. Sounds play only on actions (not icon hover).

**Scope qualification:** Some advanced 3DS editor widgets and complicated rusEFI INI expressions are not yet ported. The tests cover source behavior, not a full real-ECU compatibility certification. Actual audio output, 30 distinct live samples/s, and calibration writes must still be verified on a physical DSi and safe bench setup.

## SD card audio installation — required

Copy all 14 bundled files from:

`BrickECU-3DS-Probe/DSi/SD_Files/BrickECU/sfx/`

into the physical DSi SD card's folder:

`BrickECU/sfx/`

The directory must contain `00.pcm8` through `13.pcm8` **on the card**, not just in the build/project folder. The `.nds` file does not embed these audio files. The Soundboard reports `AUDIO: MISSING /BrickECU/sfx/` if they cannot be opened. When installing sound files, restart the app so the cached SD-file check is refreshed.

## Controls

| Screen | Input |
|---|---|
| HOME | Up/down: select workspace; A/touch: open; **L/R: 2, 3, 4 top gauges** |
| TUNING | Up/down: choose ECU-defined menu/setting, A: open, B: back |
| MAP/SETTING EDIT | D-pad: select, X/Y: increment/decrement, A: numeric keypad, L: change edit step, **R twice: protected write request**; B: cancel/back |
| CHANNELS | Choose real INI channel; L/R selects primary top gauge slot; X assigns |
| GRAPH | Select live channel and view trends |
| SOUNDBOARD | Select cue and A to play once; on-screen diagnostic identifies missing audio |
| LINK | SD, Wi-Fi, HTTP and ECU status/INI diagnostics |
| GLOBAL | SELECT: HOME, START: exit |

INI is downloaded from BrickOS to SD and a parsed metadata snapshot is cached; subsequent launches use the metadata cache when valid. The ECU connection indicator is not cleared by a transient live request failure. The client requests the ESP-supported 50 Hz source rate while aiming to display **30 distinct new ECU samples per second**, and displays the measured rate.

## Build / merge

Extract this ZIP preserving its top-level `BrickECU-3DS-Probe/` folder and merge into the existing project folder. Only `DSi/` is included: **do not overwrite your actively developed 3DS source or shared root Makefile**.

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```

Output: `Brick-Tuner-Suite-DSi.nds` under `DSi/`. Run with the working DSi loader (Unlaunch has previously worked on the user's unit).

The independent, unchanged 3DS build command is:

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

## Tests

Host C syntax and regression tests pass for HTTP/INI/SD cache, E12 socket handling, HTTP 503, map axes, bitfield editing, value bounds, signature policies, and guarded writes, as well as UI/menu/idle gauge behavior. **The actual `.nds` binary is not built in this environment**; devkitPro `nds-dev` and physical DSi hardware verification are still required. No unsafe unverified ECU writes should be performed on a running vehicle.
