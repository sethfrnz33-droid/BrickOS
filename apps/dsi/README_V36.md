# Brick Tuner Suite DSi v36 — repair INI retrieval and safe refresh

## What went wrong in v35

Version 35 erased `/BrickECU/ecu.ini` and its parsed metadata before it proved that a replacement could be downloaded. It then cleared its ECU signature while the normal INI scheduler still demanded a nonempty signature, so a bridge status response that omitted `ecu_signature` could leave the app waiting indefinitely. The old good file was already gone, so even temporary HTTP or Wi-Fi errors left the tuner with no working definition.

## Fixed

- **Connection → START → A** now means **refresh from the bridge**. The old `ecu.ini` and `ecu.meta` remain on the SD card until the new INI is completely downloaded, parsed, and committed; then they are replaced. No ECU calibration is modified.
- A successful `/api/status` response is sufficient to start a read-only INI download even if `ecu_signature` is missing. The status endpoint establishes bridge connectivity; ECU writes still require independent strict identity, engine-stopped, backup and readback verification.
- If the bridge can serve `/api/ini/file` while an ECU is offline, the INI may be downloaded for offline viewing; ECU writes remain disabled until the ECU is safely connected and verified.
- The refresh retries after transport failures and restores the previous valid local INI when the new transfer fails. The display reports **NEW INI FAILED / OLD COPY RESTORED** in that case.
- **Connection → Y** retries status and a missing INI even if the ECU is not yet online.
- If v35 already deleted the cache, v36 can download from the connected bridge when `/api/ini/file` is available; there is no need to supply a nonempty signature in `/api/status` for this read-only operation.
- If a bridge is unreachable, the user may copy a verified, matching `ecu.ini` into `/BrickECU/ecu.ini` manually and restart. Do not attempt ECU writes until the app confirms the ECU identity and live status.

## Host verification

`python3 tests/run_v36_tests.py` exercises real HTTP status/INI endpoints for a blank status signature, successful replacement, HTTP 503 with old-cache restoration, no cached INI, and an offline ECU with bridge-hosted INI. `python3 tests/run_v35_tests.py` checks preservation of the SD cache and user assets. v27 protected write and v29–v34 editor and View regressions also pass. Physical DSi/ECU Wi-Fi behavior is **not** verified.

## Build

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```

Only the DSi application is modified. No changes to the 3DS application or BrickOS ESP32 firmware.
