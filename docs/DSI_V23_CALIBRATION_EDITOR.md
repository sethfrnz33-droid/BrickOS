# DSi v2.3 — calibration editor opening/read fix

**Source package:** `Brick-Tuner-Suite-DSi-v23-Functional-Calibration-Editor.zip`, an additive DSi-only archive containing `BrickECU-3DS-Probe/DSi/` with source, Makefile, tests, and SD audio assets. The ZIP is distributed separately and is **not yet uploaded as source** to GitHub.

## User-reported failure

DSi v2.2 could show an INI tuning menu but most maps and scalar constants would not open. Its event handler attempted synchronous `/api/ts` reads before it displayed the calibration workspace; a blocking socket or denied read made the UI appear broken. The old reader also rejected bitfields even if the editor supported their type.

## v2.3 changes

- Open map/scalar/array/bitfield workspaces immediately from the INI-generated tuning tree or map list. Keep input/render responsive while loading.
- Read actual ECU Z and optional X/Y axis data using bounded `POST /api/ts` `R` requests through the existing persistent, cooperative DSi HTTP state machine (up to 256 bytes per request); display progress and report the ESP32 HTTP error body if the mandatory Z read fails.
- Optional axis read failure no longer prevents opening an otherwise valid Z map.
- Stage Z, X and Y edits independently; touchscreen numeric keypad, INI range checks, safe cancel/discard, and explicit two-stage bench-only write confirmation.
- Retain prior BrickOS write protection: live ECU/signature status, ECU stopped check, confirmed backing-page permissions, verified full-page backup, explicit arming, per-chunk readback, burn, final readback and disarm. Do not enable arbitrary live writes or bypass ECU protection.
- Avoid large temporary buffers on the DSi ARM9 stack.
- Preserve fast SD INI metadata cache, persistent Wi-Fi, 30 Hz **target** (not on-device measured), two/three/four top gauges, 10-second idle lower gauges, existing icons and SD-loaded sounds.

## Verification

Host regression tests for actual DSi input/menu handlers and mock BrickOS response paths pass: successful scalar and nested bitfield reads, map Z/X/Y data, a 640-byte multi-chunk map, local value staging and discard, HTTP 503 error reporting, and previously established protected-write rejection tests. All previous socket, INI parsing, signature and UI regressions pass. The complete project ZIP is validated for integrity and includes the 14 PCM8 SD assets.

**Not verified:** native DSi compilation, actual ECU read/write operations on hardware, every complex rusEFI INI expression, complete 3DS tuning parity or sustained 30 Hz performance. Unknown/conditional INI scale remains read-only.

## Integration safety

No changes to `apps/3ds/`, `firmware/esp32/`, existing root build scripts or shared Git history. Add only the separate `DSi/` source under `apps/dsi/` when approved and merge carefully with the current head, since other contributors are active.

Compile locally with devkitPro MSYS2:

```sh
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```

The existing 3DS build is still `cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh`.
