# Brick Tuner Suite — Nintendo DSi port and 3DS coexistence

**Status:** active DSi development, hardware integration incomplete. This document records real hardware findings and user requirements; it is NOT a validated release claim.

## Repository and build boundaries

- **Keep the actively developed 3DS edition untouched.** Never replace it with an older v62 ZIP or overwrite shared root build files. Other contributors/assistants are updating the repository concurrently.
- Add the DSi application **additively** under `apps/dsi/` (or merge its `DSi/` directory with the existing 3DS workspace when that source is imported). No force-push, history reset, or deletion of unrelated assets.
- Preserve the shared workflow and both build processes. 3DS existing command: `cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh`; DSi command: `cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make` in devkitPro MSYS2 with `nds-dev`.
- Before **each** GitHub update, read current `main`, apply only intended additions/patches on its latest head, and use a non-force update that fails if `main` advanced. Rebase/review on conflict.

## DSi hardware findings (user observed)

- Runs on Nintendo DSi from Unlaunch. Initial launches, Wi-Fi socket setup, and SD mounting had compatibility problems; those were incrementally corrected.
- ESP32 Wi-Fi AP is `brick` (not the legacy `Ignotiva S1`); HTTP bridge ordinarily `192.168.4.1:80`.
- The app has successfully downloaded/saved and parsed an INI from BrickOS via `/api/ini/file`. SD cache is `BrickECU/ecu.ini`.
- Full INI filename/prefix equality is **not** an appropriate gate for the UI: follow the working 3DS INI acceptance logic, with independent safety verification on writes.
- DSi has shown socket allocation failures (E12), an unsupported `fcntl(F_GETFL)` attempt (E88), and service-unavailable HTTP responses (488, 583). The exact response body and failing endpoint must be captured; **583 is not proof the ECU disconnected**.
- The v1.7 scheduler stabilized communication on hardware but made gauge refresh unacceptably slow. v1.8 targeted 20 Hz yet has an observed HTTP/1.1 583 response. **Sustained 20 Hz is a target, not validated performance**.

## Required DSi user experience

- **Two large top-screen live gauges**, default AFR and VBAT; large legible numeric values; configurable channels chosen from the ECU-provided INI; show *measured successful live frame rate*.
- **One coherent lower-screen TunerStudio-like launcher/navigation tree** modeled on the current 3DS Brick Tuner Suite. D-pad, A/B, and touchscreen must work; no duplicated tiny bottom navigation tabs and no inert D-pad on HOME.
- Use the **actual 3DS TS category/navigation icons** from `art/tsicons/` (converted appropriately for DS) and a readable bitmap font with clear selection/highlight.
- Port the core 3DS workflow: ECU-driven INI menus, tuning maps, map tracing/cursor indication, map/scalar editing, calibration verification/commit interlocks, live channels and graphs, sound effects. **Exclude logging and log analysis** from the DSi edition.
- Retain DSi resource constraints: no 3DS gyro/3D effect recreation as a prerequisite; no blocking network or SD work on the UI render/input loop; don't load the entire INI into limited RAM; cache completed INI to SD.
- Reuse 3DS sounds where practical; avoid noisy hover-trigger effects and do not claim audio behavior is hardware-verified until tested.

## Networking / throughput investigation

1. Compare the exact *working 3DS* HTTP connection lifecycle, request frequency, and response handling to the DSi implementation. Verify the actual endpoint/method/headers and read the ESP's response body on 583.
2. DSi must have **one well-owned TCP request at a time**, correctly call `closesocket()`, preserve the last valid samples after an error, and keep independent ECU-status/INI/live states.
3. Separate INI networking, SD writing, and incremental parsing; expose RX/SD/PARSE counters and specific error locations.
4. Test on device over minutes, not only host mocks, and report **actual good live updates/second**. If the ESP refuses repeated short-lived HTTP connections, address protocol and scheduling at both ends instead of removing ECU safety checks.

## Release discipline

- Keep 3DS and DSi source separate but available in the same repository/workspace.
- Do not assert a fix without hardware confirmation. Add tests for every observed regression (HTTP 583, DSi socket E12, GUI responsiveness, INI cache integrity, signature naming variance).
- Never silently write ECU calibration data. Preserve explicit user confirmation, safe-condition checks, verified backup/response and failure visibility.
