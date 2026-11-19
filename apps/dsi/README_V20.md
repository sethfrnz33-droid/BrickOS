# Brick Tuner Suite DSi v2.0 — live link + instant parsed INI cache

This is an additive DSi-only update. Do not replace or reset the 3DS project or the actively developed BrickOS ESP32 firmware.

## Confirmed BrickOS implementation

The currently checked-in `firmware/esp32/main/main.c` serves `GET /api/live?length=N&hz=50`. The handler subscribes the client, then returns an independently collected `BLV1` snapshot; it returns **503 Service Unavailable** if the sample is pending, stale, or memory is unavailable. `firmware/esp32/main/telemetry.c` expires live interest after 4 seconds without a request. No ECU firmware modifications are needed for the DSi changes in this release.

## Changes from DSi v1.9

- The DS-mode live client sends HTTP/1.1 `Connection: keep-alive` and reuses the same DSWiFi TCP socket for completed, fixed-length status and live requests. It also reuses a completed HTTP 503 response socket when the server permits, avoiding rapid socket reallocation (`SOCKET FAIL E12`). Chunked INI transfers and uncertain/error socket states still close normally. Wi-Fi reconnection and explicit map reads/writes release the reusable socket.
- The DSi auto-mounts SD **after rendering a boot frame** (Unlaunch SD access was confirmed by user), then loads `BrickECU/ecu.meta`, a versioned parsed-metadata snapshot, without re-tokenizing the INI. It verifies the raw `BrickECU/ecu.ini` byte count/hash and the metadata's own hash, type sizes, and bounds before accepting it. A first run from v1.9 with only `ecu.ini` falls back to a bounded parser and saves the metadata for subsequent launches; a damaged/stale cache also falls back. The raw INI remains the source of truth.
- The app launches into HOME instead of LINK when SD is present, keeping the saved 3DS-style menu layout, two large AFR/VBAT gauges, maps, tracing, channels, graph and SD sounds.
- A successful previously confirmed ECU session remains visibly connected during a transient HTTP status failure; the Link screen continues to display actual request errors and the explicit offline status if BrickOS reports it. A live GET no longer replaces the Link status line with `WAITING HTTP HEADERS` every frame.
- A one-time raw INI parser fallback processes four 1 KB buffers per frame, while keeping touch/button handling active.
- Protected write checks have not been weakened; any map write remains separately gated and verified.

## Build

From devkitPro MSYS2 with `nds-dev`:

```sh
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```

The original 3DS build remains (do not run this as part of a DSi-only update):

```sh
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

## Verification and limitations

Host regression tests pass: chunked INI transfer >900KB, parsed metadata saving/reload, changed-INI rejection, 24 repeated status GETs without new connections each time, two pending-sample HTTP 503s followed by a valid live sample, status-failure link retention, map/INI parse, and protected-write gating. The installed DSi `nds-dev` toolchain is **not present in this container**; the `.nds` binary and sustained 20Hz live gauge rate must still be verified on the user's DSi. No guarantee of 20Hz is claimed: the top screen reports measured successful updates per second.

**If startup fails after auto-mount**: launch from Unlaunch (the user's working path), and use the LINK screen to inspect SD errors. No changes to SD partitions or console firmware are required by this update.
