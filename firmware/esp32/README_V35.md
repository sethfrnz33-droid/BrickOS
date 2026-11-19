# BrickOS v3.5 — Tuning workspace

This version starts from the hardware-proven BrickOS v3.4 SD-import project. The `main/main.c`, `main/vcp_tcp_gateway.cpp`, `main/usb_sd_ini.c`, `main/ini_cache.c`, `main/protocol.c` and all other firmware/transport source files are copied unmodified. Only `web/app.js`, `web/app.css`, and generated `main/web_assets.c` differ.

- Calibration tables and settings now have search + live counts + 11 engine-system filter categories.
- Categories are derived from each INI field's name, not a hardcoded ECU pinout. The source file and ECU address remain as specified by the imported INI.
- The working SD-card INI import, CDC session, AP-only Wi-Fi, calibration write safety gates, diagnostics and logging are unchanged.
- The groups are a convenience heuristic: unmatched fields appear in Other and some names may fall in imperfect categories.
- Firmware build and real-hardware testing still required.
- Golden baseline is separately stored in the ChatGPT Library at BrickOS/Golden Backups/BrickOS_v34_PROVEN_SD_USB_GOLDEN_BACKUP.zip.
