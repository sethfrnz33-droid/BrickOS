# BrickOS v3.7 test report

- Previous hardware-verified USB CDC and SD importer are not modified; 7 source files match the proven v3.4 source byte-for-byte.
- Host C syntax with warnings-as-errors: main.c, storage.c, usb_sd_ini.c, ini_cache.c, new telemetry.c PASS.
- Embedded asset byte-matching: HTML, CSS, JS and module PASS.
- Node tests: 21 PASS, including generated Proteus F7 INI, exact menu resolution, BLV1 binary frames, framing corruption, 2248-byte output block.
- Chromium (mock ECU only): desktop and iPhone-sized responsive workbench, live scope, >10 gauges, 100 Hz target UI and full 2248-byte selection; no JavaScript exceptions PASS.
- Not verified: complete ESP-IDF build (toolchain unavailable here), actual ESP32 board, USB throughput, high sample-rate stability, real active full-block polling, device reboot behavior.
- On hardware, begin in 20 Hz Fast mode and observe live_actual_hz and live_samples_failed before raising the target.

Documentation and measurements are not performance guarantees.
