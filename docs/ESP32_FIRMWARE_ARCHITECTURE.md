# ESP32 firmware architecture: uploaded v4.8 source

This document explains the **uploaded** project rather than a proposed later generation.

## Major modules

| File/folder | Purpose |
|---|---|
| `main/main.c` | ESP entry point, open Wi-Fi AP, HTTP server endpoints, status, authorization, editor backup, guarded writes, and bench-only simulator controls |
| `main/vcp_tcp_gateway.cpp` | USB host stack, CDC/VCP transport, direct rusEFI USB communications, diagnostic counters and TCP bridge (port 2000) |
| `main/protocol.c`, `.h` | rusEFI/TunerStudio request framing and shared communication tracking |
| `main/telemetry.c`, `.h` | Read-only live channels and sampling helpers |
| `main/ini_cache.c`, `.h` | SPIFFS INI upload/cache, match checking, 1.2 MB limit; AP-only network mode |
| `main/usb_sd_ini.c`, `.h` | Optional read-only import of a matching ECU INI over USB MSC LUN1 |
| `main/storage.c`, `.h` | Logging and storage, internal SPIFFS by default or optional ESP SPI SD |
| `web/` | Browser app: UI, CSS and JS logic; regenerated into `main/web_assets.c` |
| `tools/generate_web_assets.py` | Rebuilds C byte-array assets from `web/` when UI files change |
| `sdkconfig.defaults`, `partitions.csv`, CMake files | Target, partitions, features and ESP-IDF dependency configuration |
| `tests/` | Host syntax, browser/Node tests and sample firmware INI fixtures |

## Interface contracts

- Wi-Fi AP: SSID **`brick`**, no password, default IP **`192.168.4.1`**; HTTP port **80**.
- TunerStudio TCP bridge: **`192.168.4.1:2000`**.
- Diagnostics `GET /api/status`: `brick_os_build` **v4.8**, USB readiness, ECU connection/signature, INI cache and other counters.
- Live readout: `GET /api/live`.
- TunerStudio request exchange: `POST /api/ts`.
- INI: `/api/ini/status`, `/api/ini/file`, `/api/ini/upload`.
- Logging, calibration backup, arm/disarm and bench-only simulator routes are defined in `main/main.c`. Their existence **does not** imply unattended engine-safe operation.

## v4.8 identity and special case

`README_V48.md` fixes a misleading preflight error: instead of reporting every preflight failure as a missing INI, it distinguishes unavailable matching INI, USB RPM read, nonzero RPM and battery voltage. It has known RPM/VBatt output-channel fallback descriptors **only** for `rusEFI master.2026.11.09.proteus_f7.2735508644`. Other firmware builds require their own matching INI; offsets must never be guessed.

## What v4.8 does NOT contain

- The v4.9 read-only VE learner, v4.11 guide and v4.12 motion-evidence experiments are **not part of this uploaded v4.8 ZIP**.
- The full INI inventory/dependency graph and auto-calibration coordinator are currently in `prototype/` and `docs/` as separate planning/prototype work.
- This uploaded release is not a CAN-based ESP firmware path; it uses **ECU USB CDC**.
- 3DS Brick Tuner Suite source code is not included in the uploaded ZIP; `apps/3ds/` is reserved for its actual source, not a fabricated placeholder implementation.

## Validation evidence

On an available host environment, `bash tests/run_host_syntax.sh` completed without error and `node --test tests/*.test.mjs` completed with **26/26 passing**. `tools/generate_web_assets.py` reproduced the supplied `main/web_assets.c` byte-for-byte. ESP-IDF cross-compilation, firmware flashing, USB/MSC hardware behavior and ECU interaction have **not** been verified in this environment.
