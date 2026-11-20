# BrickOS Supported INI Baseline and Fast Phone Loading

**Status:** Browser/ESP cache optimization implemented in source; GitHub host regression tests, not yet timed on a physical ESP/phone. No DS, DSi, ECU transport, calibration-write, or engine-operation changes.

## Scope decision

The supplied **2026-11-09 generated Proteus F7 INI**, signature `rusEFI master.2026.11.09.proteus_f7.2735508644`, is the **initial reference definition** for BrickOS's INI-driven phone guide and tuning interface. The program continues to validate the **live ECU signature** before enabling calibration access. The file is a definition, not a statement of the active tune or installed engine hardware.

**Do not expand into universal rusEFI/TunerStudio support.** Unrecognized firmware builds, newly introduced strategies, undefined INI expressions, physical features absent from the supported Brick hardware, and arbitrary third-party ECU configurations are outside the supported, verified project scope. Seth may display generic educational material, but must return `unsupported or unknown` rather than fabricate an INI binding or a capability. General-purpose parser improvements may be accepted when justified by this baseline.

GDI/mixed injection remain unsupported in BrickOS Guided Fuel Tuning, regardless of optional fields in the INI.

## What the DS already does

The 3DS project saves its verified INI to `sdmc:/BrickECU/ecu.ini` and reloads it from the console SD on subsequent launches, checking against the ECU signature and supporting explicit refresh. DSi likewise saves a local SD copy. A phone cannot access the Nintendo SD filesystem directly.

## Phone architecture

```
       ECU SD card
            |
        STM32 USB MSC LUN1 (when actually available)
            |
      ESP32 read-only SD importer  -- live signature comparison
            |
     /logs/ecu.ini in ESP SPIFFS
            |
     SHA-256 (once on boot or after successful cache replacement)
            |
       ESP HTTP /api/ini/status  -> cached, size, sha256
            |
      Phone browser IndexedDB: raw INI + already parsed definition
            |
     Same verified original INI field decoder and manual editor
```

If the ECU does not expose USB MSC LUN1, the existing **manual phone upload** remains the supported fallback. The ESP does not write to or format ECU SD, interrupt SD logging, or change the native USB CDC tuner route.

## What was slow and what changed

**Previous:** at app startup, the phone immediately fetched and parsed about 944 KB from ESP SPIFFS; then the ECU signature handshake called `resolveLiveIni`, which fetched and parsed **again**, even when exactly the same definition remained loaded. Reopening the phone web app repeated this work.

**Now:**
- ESP calculates a stable SHA-256 of its saved definition on initialization and after successful import, upload or deletion, and exposes it in `GET /api/ini/status` and `GET /api/status`.
- Browser first checks the lightweight hash/size metadata, then reuses **one** IndexedDB record containing the parsed definition, raw text and original reported size.
- If that browser copy matches the current ESP hash, size and expected **live ECU signature**, it skips **both** the ~944 KB Wi-Fi transfer and the full INI parse. Reconnection in the same browser session also reuses the in-memory definition.
- Loader is **single-flight**: simultaneous startup, reconnect and status events do not each trigger their own download/parser.
- After ECU SD imports, manual uploads or deleting the saved INI, the content hash changes and stale parsed definitions are rejected. A different ECU signature also rejects the cache. A cached definition never authorizes calibration writes on its own.
- No IndexedDB, quota failure or unsupported older ESP? Fall back to the existing SPIFFS download and parse. A full initial download is unavoidable when neither cache has the INI.

The browser must check its own `safeSignature()` before it considers a decoded field live. No hash substitutes for that identity check.

## Status and verification

Implemented files:
- `firmware/esp32/main/ini_cache.c`, `ini_cache.h` — SHA-256 of the saved INI, recomputed on cache change/boot; constant-size buffer.
- `firmware/esp32/main/main.c` — new metadata keys and `/ini-fast-cache.mjs` route.
- `firmware/esp32/web/ini-fast-cache.mjs` — optional persistent browser cache, single stored definition with exact hash/signature/status checks.
- `firmware/esp32/web/app.js` — existing automatic ESP SD-to-phone path now reuses parsed content and avoids repeat work.
- `firmware/esp32/tools/generate_web_assets.py` and `firmware/esp32/main/CMakeLists.txt` — include new browser source in the ESP embedded assets.
- `firmware/esp32/tests/ini-fast-cache.test.mjs` and host-only SHA header — verify validation and fallback behavior.

**Not tested on hardware:** first import time, actual network transfer improvement, phone storage behavior under iOS private mode, ESP32 compiled binary size/heap, actual MSC LUN1 mount and on-device sustained throughput. These must be measured on the target board.

## Limits

- This is **not** a live VE-learning/autotuning change, nor a step toward running software directly on the STM32. It is a source-loading optimization for the **ESP-hosted browser**.
- Phone IndexedDB does not work like an SD filesystem and may be cleared by the browser; the authoritative persisted copy is ESP SPIFFS, whose source is ECU SD (or a manual upload).
- Hashing the stored INI is content identity, **not operator authentication** or proof of physical hardware.
- A signature that happens to match is necessary but does not independently verify ECU hardware, calibration or all INI semantics.
- The existing DS/DSi source, connectivity, SD behavior and manual tuning protocol are unchanged.

## Suggested non-operational validation

Inspect the source metadata and compare:
- First browser visit on an empty local browser cache: one full INI download and parse, followed by one IndexedDB save.
- Second visit with the same ESP cached file: metadata request plus local parsed-definition retrieval, **no full INI download**.
- Reconnect same ECU: no redundant INI download.
- Replace cached INI while retaining ECU signature: different SHA-256 causes refresh, not reuse.
- Connect ECU with different signature: reject cached mapping and require matching definition.
- Disable IndexedDB: existing network path still works.
- MCU storage or USB SD unavailable: UI remains usable in no-definition mode; no made-up calibration offsets.

Do not advertise a numerical speedup until timed on a target phone/ESP combination.
