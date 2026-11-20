# BrickOS v3.9 — Calibration staging, guarded USB write path, and fast gauges

**Project:** ESP32-S3 BrickOS web app for rusEFI USB; based on the uploaded v3.8 project. The complete project folder is inside the release ZIP.

## What changed

- **Live ECU definitions:** decode the current rusEFI-generated `pageIdentifier` values as little-endian two-byte identifiers (e.g. `"\\x00\\x01"` → 0x0100). This is necessary for correct page 2+ reads and all page operations. Reject unknown/invalid pages rather than guessing.
- **Editable calibration tables and axes:** VE-style maps use the actual `[TableEditor]` `xBins` and `yBins` declarations, and all three fields are editable from a single table screen. Grey headers indicate axis values. For an axis whose INI scaling expression cannot be evaluated, display and edit explicitly labeled *raw ECU units*; no inferred engineering conversion is used.
- **Multi-field staged session:** switch between tables or settings without losing unsaved edits. Keyboard navigation, value editing, increments, smoothing, undo/redo, portable patch files and page/all-staged apply are available. Staging is local until explicitly applied.
- **Guarded calibration writing:** build-time write-enable, **physical bench jumper**, matching live ECU signature, full-page backup and fresh confirmed 0-RPM checks are required. Browser arm lasts 60 seconds and server denies any `C`/`B` command after authorization expires or the USB session disconnects. Firmware defaults to read-only.
- **Conflict checking and readback:** before the first write, compare all staged original bytes against fresh ECU reads. Write small contiguous `C` chunks, verify each chunk, perform **one `B` command per page only if declared by that INI**, and verify again after burn. Pages with no burn command are marked RAM-only/nonpersistent; failed checks halt later writes.
- **Fast live gauges:** binary BLV1 telemetry continues to be used. The fast mode requests only the bytes covering displayed gauges, scope and RPM (up to 768 bytes); full mode is still available. Samples can be requested with a **100 Hz target**, but the graph is drawn at about 30 fps to reduce browser load. Actual measured Hz, USB latency and errors are visible in the ECU status panel.
- **Correct patch undo:** importing a `.btune` patch now creates a structured undo entry compatible with cross-field undo/redo. Changing an INI clears stale calibration edit buffers.

## Before any real ECU writes

1. The firmware's write feature is **disabled by default**. `main/Kconfig.projbuild` exposes `CONFIG_BRICK_ALLOW_CAL_WRITES` and `CONFIG_BRICK_WRITE_ARM_GPIO`. A board-specific **confirmed free and accessible** jumper input GPIO must be configured by the hardware owner; no GPIO was assumed. Never repurpose a live or reserved pin.
2. Use this write path only with an engine stopped on a safe, stationary test bench. The UI requires a current zero-RPM reading and issues an additional USB RPM read immediately before modifying each page. Do not use it for live/road tuning.
3. Connect a compatible ECU via the already working native-USB gateway, load/verify the exact INI for its live firmware signature, open a table/setting, and **download the complete calibration page backup**.
4. Stage changes locally. With verified bench authorization present, explicitly arm the 60-second write window, then choose **Verify & apply page** or **Verify & apply all staged**. Each change is checked and read back automatically. Confirm on the actual ECU before relying on any changed calibration.
5. If the page has no burn command, the write may be **RAM-only**; it is not equivalent to persistent storage. A successful communication exchange does not demonstrate correct engine behavior or calibration validity.

## Build

- ESP-IDF v5.3.1 / ESP32-S3 according to this project's original configuration.
- Generate the embedded app assets after modifying `web/*`: `python tools/generate_web_assets.py`.
- Build in a correctly configured ESP-IDF environment using `idf.py build`. Firmware image generation/flashing and a physical ECU connection **were not tested in this environment**.
- Default open Wi-Fi AP from the uploaded project is `brick`, and the workspace is served from `http://192.168.4.1/`.

## Tests in this release

- `node --test tests/*.test.mjs`: **26/26 pass**. Includes live INI fixtures, identifiers, axis metadata, multi-field write plans, overlapping conflicts, binary telemetry, and patch validation.
- `bash tests/run_host_syntax.sh`: host syntax checks of C gateway, USB/SD and telemetry sources **pass**.
- `bash tests/run_assets_host.sh`: verifies compiled embedded assets against `web/*` (run after generating assets).
- `python tests/v39_browser_smoke.py`: headless Chromium with a **synthetic** ECU implements `S`, `R`, `O`, `C` and `B` responses. Covers live handshake, both axes, cross-navigation staged changes, backup, authorization, verified flash burn, stale conflict rejection, dashboard, and mobile width. **Not a physical ECU benchmark.**

## Known scope boundaries

This release is a useful TunerStudio-like calibration editor, **not full feature parity** with TunerStudio: it does not claim complete rusEFI INI expression evaluation, live auto-tuning/VE analyze, every specialized TunerStudio editor, or verified production use on your hardware. Some conditional axis conversions intentionally stay in raw units. Neither 100 Hz target nor a mock-browser test proves actual sample throughput or ECU flash write success on your board.

Protocol references:
- rusEFI source: https://github.com/rusefi/rusefi/blob/master/firmware/console/binary/tunerstudio.cpp
- rusEFI INI template: https://github.com/rusefi/rusefi/blob/master/firmware/tunerstudio/tunerstudio.template.ini
