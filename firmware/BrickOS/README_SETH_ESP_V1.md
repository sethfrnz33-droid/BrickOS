# Seth ESP educational agent: integration v1 (feature branch)

**Status: source integration on a review branch. The actual ESP-IDF build and
physical hardware test are pending. Do not deploy this branch as production
tuning firmware without completing those checks.**

This first step links the existing standalone, bounded C11 Seth agent
from `prototype/seth-agent/` into `firmware/BrickOS/main/`. The original
author-written lesson catalogue is compiled to a static C array during ESP-IDF
configuration; there is no cloud inference, invented calibration data, or new
ECU command path.

## Access from the phone

- Join the existing BrickOS Wi-Fi network and open `http://192.168.4.1/`.
- Choose **Ask Seth** in the navigation, or open `http://192.168.4.1/seth`.
- The browser consumes the same JSON packets a later 3DS client can display.
- **Manual Tune, Guide (legacy browser-side), and the USB bridge stay separate.**
- This is a **single, shared, volatile educational session**. No authenticated
  user/profile identity or persistent archive exists on the open access point.

## Portable API contract

`GET /api/seth/v1/view` returns:

```json
{
  "schema": "brickos.seth.esp.view.v1",
  "ok": true,
  "agent": {
    "title": "Welcome to Ask Seth",
    "revision": 0,
    "read_only": true
  },
  "source": {
    "ecu_connected": false,
    "ini_cached": false,
    "ini_signature_matched": false,
    "telemetry_fresh": false,
    "role_binding": "unverified_no_sensor_values_decoded",
    "calibration_authority": false
  },
  "session": "shared_volatile_education_only",
  "engine_archive_saved": false,
  "live_diagnosis_available": false,
  "ecu_writes_available": false
}
```

*The abbreviated example omits existing agent packet fields and additional
telemetry/INI metadata. It illustrates the schema, not a measured ECU status.*

`GET /api/seth/v1/lessons` returns `schema`,
`read_only`, `stages[]` (zero-based chapter number and ID), and `lessons[]`
(ID, chapter ID, title and version). This is the source of truth for client
lesson selection, so phone and 3DS do not embed hardcoded lesson lists.

`POST /api/seth/v1/action` accepts up to 448 bytes of JSON:

```json
{"revision":0,"action":"full_tour","value":""}
```

Only education actions are allowed: `home`, `full_tour`, `stage`,
`continue`, `lesson`, `study_topic`, `continue_topic`, `depth`,
`interaction`, `explain_alternate`, `check_start`,
`check_answer`, `i_understand`, `report`. A successful action
returns the complete updated view. A stale revision returns HTTP 409.
There is no `set_fact` or `diagnose` remote action because the current
AP is open and there is no private, authenticated profile storage.

The service only **reads**:
- USB-ready status and session signature (same existing BrickOS discovery);
- cached INI SHA-256 and **exact live-ECU TunerStudio signature match**;
- already-sampled telemetry sequence, freshness, sample count and rate.

**It does not** subscribe to new USB data, interpret source bytes as real
sensor values, calculate VE/spark changes, make a physical diagnosis, persist
engine hardware claims, or send any ECU command. A matched INI signature and
a fresh USB sample are not enough to validate physical sensor identity.

## Build / host tests

From the BrickOS repository root:

```sh
python3 firmware/BrickOS/tools/generate_web_assets.py
make -C prototype/seth-agent clean test
node --test firmware/BrickOS/tests/seth-esp-http-contract.test.mjs
cd firmware/BrickOS
idf.py set-target esp32s3
idf.py build
```

The generated `main/web_assets.h` and `main/web_assets.c` are refreshed by
ESP-IDF CMake configuration from the current web files, including `seth.html`.
The C lesson catalogue is generated into the ESP-IDF component build directory
from `design/education/lesson-catalog.v1.json` and is not hand-maintained.

## Known unresolved work

1. Confirm ESP32-S3 flash/heap/stack budget, boot and web-route registration
   under the exact user's firmware/ESP-IDF revision.
2. Verify actual USB/INI identification and timestamp-quality metadata on
   target hardware without disrupting the existing fast manual telemetry.
3. Implement an independently verified INI semantic role resolver before
   attaching named sensor values or turning evidence into diagnostic findings.
4. Implement an authenticated, versioned, power-loss-safe project profile
   storage strategy before enabling `set_fact` or private user answers.
5. Implement a later DS/3DS UI client against these same view/lesson/action
   packets. The imported `apps/3ds/` on GitHub predates the user's local v104
   project, so this branch deliberately does not overwrite its UI sources.
6. Preserve the standalone simulator's synthetic evidence tests, and only
   add live evidence review with measured source timing and physical validation.

**Educational progress is always separate from physical engine readiness
or safe calibration evidence.**
