# BrickOS v4.3 — iPhone Safari write-path interruption fix

## Actual report and evidence

User's iPhone screenshots show the complete page-1 prewrite backup download, then `ECU write stopped: Load failed` with the header changing to `NO ECU`. The attached backup is 17,816 bytes, the exact page-1 size declared by the included generated rusEFI Proteus F7 INI. This proves the backup request succeeded, NOT that the `C` write or `B` burn was sent or accepted. Safari/WebKit reports generic network request failures as `Load failed`. We cannot prove from the screenshot alone whether the download dialog, iPhone Wi-Fi roaming, ESP32 HTTP reset, or USB disconnect was responsible.

## What changed

- Automatic prewrite backups are saved synchronously to the browser's same-origin persistent storage, with ECU signature, page number, length, timestamp, and CRC32 readback validation. **No automatic iOS download dialog interrupts an active write operation**.
- A **Download saved backup** button exports the saved binary, and **Backup & download** explicitly saves and exports it on request. Storage failure cancels the write before `C`/`B`.
- **Every write operation takes a NEW complete page backup**. The former session-level `S.backups.has(page)` allowed subsequent writes to skip the backup incorrectly.
- On Safari `Load failed`, only idempotent ECU `R` and `O` requests are retried; `C` and `B` remain non-retrying. Diagnostics identify the precise stage and state whether a write *could* have been transmitted.
- USB/ESP32 session updates avoid concurrent browser status polling while the calibration write is in progress.
- Writes remain compiled and enabled, subject to exact ECU firmware signature, a direct 0 RPM check, and temporary transaction authorization. There is no additional jumper or build-time lock.
- Build status `/api/status` reports `brick_os_build: v4.3` after the new ESP32-S3 firmware is flashed.

## Install and test

This is complete **ESP32-S3 firmware source**, not a precompiled image. Build with the installed ESP-IDF configuration, regenerate `main/web_assets.c` with `python tools/generate_web_assets.py`, then flash the verified ESP32-S3 target. Do not flash the STM32 ECU with this project. Verify `/api/status` reports `v4.3` before retesting.

On a stationary stopped-engine bench, open a tuning table, stage an edit, click **Verify & write**, then review the result. The initial backup is saved locally without a download popup; use **Download saved backup** after the operation to export it. If the browser still reports a network failure, note the *phase* printed in the persistent calibration-session error. The ESP32 `/api/status` endpoint exposes USB disconnection, Wi-Fi client, request and reset counters to distinguish transport from ECU problems.

The browser's persistent origin storage is not a substitute for long-term backup archives; export the binary and keep the original backup safely. Clearing browser website data deletes this locally stored copy.

## Verification limits

Tests cover an HTTP/USB simulator in Chromium, page 1 backup persistence, actual INI parser and map axes, three field edits, `C`/`B` sequence and readback, simulated Safari `Load failed` on an idempotent read, simulated ambiguous write failure with no automatic retry, stale-edit conflict, and disabled older firmware. Tests do **not** establish that writes work on the physical ECU or that iPhone Wi-Fi remains connected. Those require real bench verification.
