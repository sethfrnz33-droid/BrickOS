# Brick Tuner Suite DSi v2.1 — 30 Hz target, 3DS-style idle gauges, expanded tuning

This is an **additive DSi-only source update** on the confirmed-working v2.0 networking base. It is not a replacement for the actively developed Nintendo 3DS project or BrickOS ESP32 firmware.

## Dashboard and telemetry

- The upper screen retains **two large gauges**, initially AFR and battery voltage. Pick a channel in **CHANNELS**, use L/R to choose a top-screen slot, and **X** to assign that real INI-defined channel; selections now persist in `BrickECU/dsi-gauges.cfg` on SD.
- The DSi targets **30 fresh live ECU samples per second** by polling once every two DSi VBlank ticks, using BrickOS's supported `hz=50` request. The graphics are redrawn at up to 30 FPS, and network parsing advances up to four bounded nonblocking steps per video tick.
- **HTTP-successful duplicate BLV1 sequence numbers no longer trigger a transport backoff.** Identical samples remain valid but do **not** count as fresh updates. Status, INI, and live-state errors remain independent.
- Every main-loop iteration now has **only one** VBlank wait, including rendering; earlier code waited twice on some frames and slowed requests unintentionally.
- **After 10 seconds without controls**, HOME/TUNING/MAPS browse screens replace the lower menu with up to **six additional real ECU gauges**. They follow the 3DS priority list, exclude the two upper gauges by ECU field identity, and avoid false duplicate labels. The first key or touch restores the menu without executing a hidden action. This never covers an active map edit, panel or unsaved change. The larger capture is requested only when those idle gauges are showing.

**Rate qualification:** 30 Hz is a scheduling target, **not** a measured DSi throughput guarantee. It depends on actual DSWiFi/HTTP response latency and ECU sample delivery. The HOME screen shows received *distinct* sample rate. Test it on physical DSi hardware with BrickOS before claiming 30 Hz operation.

## Tuning, maps and tracing

- All valid INI-generated categories and nested panel links remain accessible from TUNING with D-pad/touch. Nested dialogs now return to their parent panel on B.
- Map tables, curves, and scalar/array fields retain a real ECU read, local staged values, axis data when defined, and a live cell tracer using the INI-defined runtime axis fields.
- Tuning menu entries that directly reference real scalar/array constants can now open the editor without requiring a separate dialog reference. INI dialog array fields also use the existing editor (within the 1024-cell limit).
- On an opened map/setting: **D-pad** selects a cell, **X/Y** increments/decrements the selected value, **L** changes the edit step, **A** opens the touchscreen numeric keypad; the lower screen also has `-`, `+`, `STEP`, and `TRACE` buttons. Numeric values are scaled and checked against the ECU INI limits, and only the in-memory staged buffer changes until explicit confirmation.
- **R twice** requests the existing bench-only protected burn sequence. The client verifies ECU session and signature, fresh stopped-engine RPM, page write/burn permissions, baseline readback, ESP32 page backup, explicit arm permission, per-write readback, final burn/readback, and disarm. **Do not attempt write tests with a running engine.** This logic is unchanged from v2.0 and is host-tested, not proven on DSi hardware.
- **B** cancels. Dirty map edits require a second B to discard them; keypad B cancels only the entry, without discarding the existing staged map.
- DSi exclusions are still logging/log analysis and 3DS-only gyro/3D effects. The DSi does **not** claim full TunerStudio/3DS parity for complex bits/enum choices, power-cycle requirements, multi-cell math, offline tune file/project export or hardware-verified burns.

## Audio, icons and release merging

Original 3DS category icons and 14 PCM8 event sounds are retained. Copy `DSi/SD_Files/BrickECU/sfx` to `BrickECU/sfx` on the DSi SD card. Audio plays on confirmed actions, not merely hovering over icons.

The archive contains **`BrickECU-3DS-Probe/DSi/` only**. Extract into your Downloads folder, merging the DSi subdirectory; do not replace the root 3DS Makefile, `apps/3ds`, or BrickOS firmware. Run from devkitPro MSYS2 with `nds-dev` installed:

```sh
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```

3DS command stays separate and unchanged:

```sh
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

## Verification

- Compiled under host GCC with `-Werror` where test harnesses support it; INI parsing, HTTP stream, persistent socket, server HTTP 503 retry behavior, repeated frame parsing, idle channel selection, keypad/staging, nested navigation, SD gauge persistence, and protected writes were tested.
- This environment does not have the devkitPro ARM9 cross-compiler or a DSi; **no `.nds` or end-to-end hardware verification** is claimed.
