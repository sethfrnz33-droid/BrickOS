# Brick Tuner Suite — DSi v2.3 calibration editor fix

This is the **DSi-only** source update for the existing Brick Tuner Suite workspace. Do not replace the actively developed 3DS source or BrickOS ESP32 firmware with the historical v62 archive. A working v2.0+ persistent Wi-Fi client and cached SD INI are retained.

## Why v2.2 tuning entries did not open

The DSi application previously performed a synchronous ECU calibration read directly inside its button/touch event handler. Socket waits and read failures could prevent the editor from appearing, leaving maps and scalars seemingly inaccessible. The old bitfield path also rejected ECU reads before an editing screen could open.

## v2.3 changes

- Opens INI-defined maps, curves, standalone numeric scalars/arrays, and supported bitfields directly from the Tuning or Maps menu. The workspace opens immediately instead of blocking on a TCP round trip.
- Reads the actual ECU calibration bytes using bounded, **nonblocking** `POST /api/ts` commands (`R`, at most 256 bytes per request), through the same persistent HTTP connection as the working dashboard.
- Loads map Z values and optional X/Y axis data separately. If optional axis data fails, the Z calibration still opens. Failed mandatory ECU Z reads show the actual HTTP/transport error; no fabricated values are displayed.
- Allows staging changes to a map Z cell or X/Y axis bin with a touchscreen numeric keypad, as well as numeric scalars and available bitfield options. Values are checked against INI ranges when supplied; unknown/conditional scaling is read-only rather than guessed.
- Keeps all staged data local until an explicit write. The original protected write path still demands ECU session match, stopped RPM, page permissions, verified full-page ESP32 backup, explicit arming, chunk write readbacks, burn acknowledgement and final verification. **Bench-only editing; never test writes on a running engine.**
- Moves large synchronous calibration scratch buffers from the ARM9 call stack to shared storage.
- Preserves prior improvements: cached parsed SD INI, two/three/four top gauges, lower-screen idle gauges, live graphs, map cursor tracing, 3DS-style icons and SD-loaded sound effects. 30 Hz new-sample throughput remains a target rather than a hardware-verified result.

## Editing controls

1. On HOME, choose **Tuning** to browse ECU-provided categories or **Maps** for the table index. Use D-pad up/down and A, or tap a row.
2. The calibration screen opens at once and reports `READING ECU CALIBRATION`, followed by actual values once the ECU replies. B cancels a read.
3. Move within the opened map with the D-pad. **SELECT** switches the editable focus between available **Z, X and Y** data. **A** opens the numeric keypad. X/Y buttons increment/decrement locally, while L changes the step.
4. **R twice** requests a bench-only write of the **selected, edited field**; check the on-screen result and preserve a verified backup. This is not a live-edit/autowrite feature. Press B twice to discard unsaved edits.
5. If the ECU denies a read, the menu shows a specific error (for example HTTP 503 and its message). The UI remains responsive. If no suitable scaling formula is available, the field is displayed raw and deliberately remains read-only.

## Build

Using the devkitPro MSYS2 shell with the `nds-dev` group installed:

```sh
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```

Copy `Brick-Tuner-Suite-DSi.nds` to the DSi's SD card and launch through the working Unlaunch setup. SD event sound effects are **not** inside the ROM: copy `DSi/SD_Files/BrickECU/sfx/` to `BrickECU/sfx/` on the DSi SD card if not already present.

The separate 3DS build remains:

```sh
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

## Verification and limits

Local host regressions exercise real DSi menu handlers, a mock BrickOS HTTP server, two scalar/bitfield open paths, map Z/X/Y reads, multi-request 640-byte tables, staged edits and protected write rejection. They do **not** establish successful native DSi hardware reads, actual ECU writes, fully decoded conditional INI expressions, or complete 3DS advanced-widget parity. The native devkitARM toolchain is unavailable in this environment.
