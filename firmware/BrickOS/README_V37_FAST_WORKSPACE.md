# BrickOS v3.7 — PCLink-inspired workspace and high-rate USB telemetry

## Protection of the proven baseline
- Start point: v3.6, itself based on the user's hardware-tested v3.4 USB CDC/ECU SD import.
- The files `main/vcp_tcp_gateway.cpp`, `main/protocol.c`, `main/usb_sd_ini.c`, `main/ini_cache.c`, `main/storage.c`, `main/tsdiag_parser.cpp`, and `main/hub.h` are SHA256-identical to v3.4.
- Wi-Fi remains an open AP named `brick`; no STA mode, internet download, or external TCP tuning gateway is added.
- Calibration write gates and full-page backups remain as they were; this update's new telemetry stream is read-only.

## New data pipeline
- New independent `main/telemetry.c/.h` USB output-channel sampler, activated only by a browser requesting `/api/live`.
- Uses original `ts_request('O')` and STM32 USB CDC driver; no alternate handshake, no CAN, no USB descriptor changes.
- Captures read-only output channels into a mutex-protected buffer so browser polling never duplicates USB reads.
- High-rate target selection: 10, 20, 50, 100 Hz. Values are **requested target rates**, not certified measured throughput.
- Two binary modes: `Fast` reads up to 768 bytes; `All` reads up to 3072 output-channel bytes in up to three 1024-byte USB requests. The uploaded Proteus F7 generated INI declares 2248 bytes of output channels.
- HTTP GET `/api/live?length=768&hz=50` responds `application/octet-stream` with 16-byte little-endian header `BLV1`, `sequence:u32`, `sample_ms:u32`, `payload_len:u16`, `usb_duration_ms:u16`, then raw output bytes. Frames are exact-length-validated.
- `GET /api/status` now reports `live_active`, `live_target_hz`, `live_actual_hz`, `live_sample_duration_us`, `live_sample_age_ms`, `live_sequence`, `live_sample_bytes`, `live_samples_ok`, `live_samples_failed`.
- Sampler stops polling after four seconds without browser requests, does not start until a valid USB signature is known, and slows to 10 Hz while working in non-live pages.
- The interface falls back to the v3.6 JSON `/api/ts` transport if the binary endpoint is absent.

## Interface rebuild
- New industrial dark multi-pane dashboard with real-time gauge cards, three-channel time scope, measured USB round-trip, actual sample rate, telemetry age, and throughput in KB/s.
- Main gauge names map to actual Proteus F7 output-channel fields, including RPMValue, MAPValue, TPSValue, coolant, intake, VBatt, lambdaValue, AFRValue.
- The original TunerStudio `[Menu]` hierarchy stays generated directly from the live INI (no guessed calibration addresses); map/table editors and all current diagnostics and logging pages are preserved.
- Selectable `Fast` versus `All` output channel capture and 10/20/50/100 Hz targets. A requested 100 Hz does **not** mean the hardware can deliver that rate; the UI exposes measured actual rates.
- Mobile responsive workspace, with tests against the exact previously uploaded generated Proteus F7 INI.

## Product feature roadmap (NOT YET IMPLEMENTED)
- User-configurable tabbed/dockable pane layouts and permanent dashboards.
- Multi-table synchronized editing, additional map/3D analysis tools, heatmaps, and keyboard workflows.
- Advanced logging views (XY plots, statistics, histograms, comparisons, synchronized cursors), event markers, export formats.
- Further USB/Wi-Fi throughput improvements informed by hardware measurements; evaluate WebSocket push after baseline stability is confirmed.

## Build and validation
- ESP-IDF 5.3.1; ESP32-S3 with native USB host to STM32 USB CDC; `idf.py build flash monitor` in extracted project folder.
- `python tools/generate_web_assets.py` regenerates embedded HTML/CSS/JS after web edits.
- `bash tests/run_host_syntax.sh` checks C syntax of the existing main/SD modules; independent `main/telemetry.c` also host-compiled with `-Werror`.
- `node --test tests/*.test.mjs` validates parsing, protocol framing, generated INI, and menu selection.
- `bash tests/run_assets_host.sh` checks byte-for-byte embedded webpage assets.
- `python tests/build_preview.py && python tests/v37_browser_smoke.py` tests layout and mock telemetry with desktop/mobile Chromium.
- Full ESP-IDF build, ECU USB throughput, sustained 100 Hz, Wi-Fi stability under continuous load, and hardware behavior remain unverified here; test on the bench and use v3.4 golden backup to recover.

## Verify speed on your ECU
1. Flash and connect to open Wi-Fi AP `brick`, then open `http://192.168.4.1/`.
2. Check that the ECU and firmware-matched INI still connect/import automatically.
3. Dashboard > choose `Fast` and start at 20 Hz, then increase the target only if actual Hz and sample failures remain healthy.
4. Runtime gauges > choose `All` to capture the entire current INI output block.
5. Visit `http://192.168.4.1/api/status` and compare `live_actual_hz`, `live_sample_duration_us`, `live_sample_bytes`, and `live_samples_failed` to the requested target.
