# BrickOS v4.4 — ESP32-side calibration backup

Fix: v4.3 depended on Safari localStorage to persist a complete ECU page before writing. Some Safari sessions reject localStorage writes; no calibration write was then attempted.

`POST /api/backup` now reads the complete calibration page through the existing native USB rusEFI `R` command into a temporary file on the ESP32 mounted SD/SPIFFS log storage. It verifies length and CRC32 against rereading the stored file, atomically renames it, and responds with a page/size/checksum receipt. Browser writes only continue after verified backup acknowledgement. The saved binary can be downloaded after the operation from `GET /api/backup/get?page=N`; the browser no longer needs localStorage or a mid-write download prompt. Full backups require mounted storage with adequate free space.

This is firmware source, NOT a flashed build. Verify `/api/status` reports v4.4 before testing with the engine stopped on a stationary test bench. The C syntax and mocked-browser tests do not prove physical ECU flash writes.

The companion 3DS v1.2 build fixes separate client-side write blockers (legacy nonexistent physical-arm field, missing `/api/arm`, missing signature in `C`/`B`, and inappropriate background poll contention).
