## GPIO8 harness-isolation output testing (source updated 2026-11-19)

Output-test support is merged into `firmware/BrickOS/main/main.c` on `main`. No external firmware patch script is required. The board's **user-described** interlock uses **GPIO8 HIGH = harness 12 V isolated**; GPIO8 LOW, unstable or unconfigured means testing is blocked. The ESP32 checks this input (no internal pull-up), matching active INI, ECU USB session and fresh zero RPM before accepting a named output-test request. It exposes `/api/test/preflight`, `/api/test/start`, and `/api/test/abort`; a watchdog attempts Abort if the interlock becomes unsafe, connectivity is lost or the short timeout expires. Main/starter relay and arbitrary Z command selectors are excluded.

**Validation:** Source-level checks passed; ESP-IDF cross-compilation, GPIO8 electrical isolation and actual ECU/3DS hardware testing have **not** been performed. Firmware cannot independently prove physical harness isolation. Do not use output testing until the board's power cutoff, GPIO8 bias and automatic Abort are validated in an isolated bench setup.

# BrickOS ESP32-S3 firmware — v4.8 baseline

This directory is the complete, original ESP-side v4.8 release uploaded as `BrickOS-v48-TRIGGER-INI-FIX(2).zip`. The code is for an **ESP32-S3**, not the STM32-based ECU; it is built using ESP-IDF, not Arduino IDE. Guided Tuning v4.9-v4.12 is **not merged into this v4.8 source**.

This project serves a browser tuning UI, bridges TunerStudio-over-Wi-Fi to native ECU USB CDC, caches matching rusEFI INIs, supports calibration read/write with guarded editor workflows, and optionally reads an INI from a USB mass-storage-equipped ECU SD card. See [the full installation and wiring guide](../../docs/ESP32_INSTALL_AND_CONNECTIONS.md) for physical setup, and [firmware notes](../../docs/ESP32_FIRMWARE_ARCHITECTURE.md) for internals.

Build commands from an **ESP-IDF 5.3.1 environment**, within this directory:

```powershell
python tools/generate_web_assets.py
idf.py set-target esp32s3
idf.py build
idf.py -p COM7 flash monitor
```

Replace `COM7` with the actual ESP32 programming port. Flashing should use the ESP32 **programming** connector; the ECU-facing USB-OTG port is used as a **host** at runtime. Do **not** flash this image to the ECU. An ESP-IDF 5.3.1 toolchain and physical ESP32-S3 are required; this repository's available tests are not a substitute for an actual cross-build or hardware verification.

Join the **open** Wi-Fi SSID `brick` and visit `http://192.168.4.1/` or `http://192.168.4.1/api/status`; the native TCP TunerStudio bridge listens on port `2000`. No CAN connection is required for this version. Its open AP is not appropriate for use near untrusted clients; disconnect sensitive actuators and use an isolated bench setup for integration tests.

**Project status:** Source-preserving import of the user's v4.8 code. Review `README_V48.md` for v4.8 fixes, `README_V47.md` for the bench-only simulator restrictions, and `README_LEGACY_V42.md` to understand earlier release history. `main/web_assets.c` is generated from `web/` and must be regenerated after UI edits.
