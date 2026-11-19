# Brick Tuner Suite DSi v2.0 — SD startup and persistent HTTP

**Release status:** DSi-only source package prepared; native devkitARM build and hardware operation not verified. No ESP32 or 3DS firmware/source is changed by this update.

## Actual BrickOS telemetry contract

Verified from `firmware/esp32/main/main.c` and `firmware/esp32/main/telemetry.c`:

- `GET /api/live?length=<bytes>&hz=50` subscribes to the firmware's existing background telemetry collector; the response is a binary `BLV1` frame.
- HTTP **503 Service Unavailable** means no usable sample is ready (including first sample pending or stale). It is not an ECU write-authorization gate.
- Subscription interest expires after **4 seconds** without another request, so multi-second exponential retry delays can prevent live sampling from ever warming up.
- The server accepts HTTP/1.1 and the DSi should reuse a completed, non-chunked response connection when possible rather than opening a fresh DSWiFi socket for every gauge poll.

## DSi v2.0 source changes (separate package)

- The cooperative DSi HTTP client requests `Connection: keep-alive`, reuses one TCP socket across complete fixed-length status and live responses, including sample-pending 503s with known lengths, and releases it on real transport error, Wi-Fi reset, or synchronous map operations. This addresses frequent `SOCKET FAIL E12` caused by connection churn.
- After an initial rendered frame, the DSi mounts the confirmed working Unlaunch-accessible SD and loads a **versioned parsed INI metadata snapshot** from `BrickECU/ecu.meta`. It validates raw `BrickECU/ecu.ini` size/checksum and snapshot checksums before publishing metadata. First run or changed/corrupt files fall back to parsing the raw INI once, then regenerate the metadata snapshot.
- Boot opens the HOME workspace immediately with the cached tuning menu instead of always opening LINK and repeating the full text parse. The Link screen no longer changes the confirmed ECU state to `WAITING` during each new live request.
- Retains two large AFR/VBAT gauges, actual measured live update-rate reporting, INI-driven map/tracing, D-pad/touch navigation, icons, and SD sound effects.
- ECU write authorization and verification are not relaxed. Sustained 20 Hz remains a **target**, not a measured on-device result.

## Verification

Ten host regression scripts passed, including >900 KB INI streaming, snapshot round-trip and changed-INI rejection, 24 requests over reused TCP, two 503 responses followed by a valid sample without reopening sockets each time, linked status retention, parser, map axes, and protected writes. Native DSi build and DSi/ESP32 hardware tests are still required.

Build at `BrickECU-3DS-Probe/DSi` using devkitPro MSYS2:

`cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make`

**Concurrency:** do not overwrite the actively developed `apps/3ds/` or ESP32 firmware. The DSi source is distributed in a separate merge-safe ZIP; this documentation-only commit does not claim source was pushed.
