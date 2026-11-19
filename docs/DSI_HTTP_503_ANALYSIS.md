# DSi live polling: BrickOS HTTP 503 investigation (2026-11-19)

**Source reviewed:** `firmware/esp32/main/main.c` (`live_get`) and `firmware/esp32/main/telemetry.c` (`brick_live_subscribe`, `live_task`). This analysis describes the checked-in BrickOS firmware; confirm the installed ESP32 binary matches before claiming hardware resolution.

## Verified behavior in checked-in source

`GET /api/live?length=<bytes>&hz=50` subscribes the client and then attempts to copy the latest telemetry sample. The handler returns **HTTP 503 Service Unavailable** with one of these errors if it cannot supply a valid frame:

- `No telemetry memory`
- `Waiting for first ECU telemetry sample`
- `Telemetry sample pending or stale`

Sample rejection includes length mismatch and a sample age above **1,200 ms**. The live subscription is considered inactive after **four seconds** without a renewed request; on renewed interest after that timeout, the firmware clears the cached sample. This is an important protocol property, not an ECU write safety lock.

## DSi feedback-loop regression

Before v1.9, the DSi networking scheduler doubled its retry delay for *every* failed live request: 120, 240, 480, and up to 960 video frames. At ~60 frames/s, this can exceed the ESP's four-second subscription lifetime, meaning a 503 caused by a pending sample may repeatedly lead to further 503s after each long pause.

The DSi v1.9 candidate instead distinguishes:
- **Live HTTP 503:** retry after **24 video frames (~400 ms)** to maintain interest while the ESP telemetry worker starts or refreshes; do not invalidate the previously confirmed ECU session.
- **Socket errors / other failures:** retain bounded exponential backoff to avoid DSWiFi resource exhaustion.
- **Successful live response:** clear the matching failed-request diagnostic and measure true successful updates/second.

Status and INI progress are separate; a live-data 503 must not show `ECU DISCONNECTED` unless `/api/status` explicitly reports it.

## Remaining hardware validation

A 20 Hz **request schedule** is not proof of 20 Hz **new samples**. Measure successful response count and ECU `live_actual_hz`, not drawing frame rate. Report the exact HTTP status, endpoint, and response body; the user has read a DSi screen as `583 Service Unavailable`, whereas the currently checked-in firmware returns `503 Service Unavailable`. Confirm the displayed three digits on hardware and compare the running ESP build.

No changes to `firmware/esp32/` are proposed by this note. Existing 3DS project and running firmware must remain intact during DSi integration.
