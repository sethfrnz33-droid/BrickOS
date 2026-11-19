# BrickOS v2.1 — USB enumeration lifecycle fix

- Removed repeated `usb_host_device_free_all()` from native USB connection-manager retry path.
- Close stale CDC/VCP device handle after disconnect before reopening, under existing USB TX mutex.
- USB host stack starts at boot and retries STM32 device detection without a TCP client.
- New /api/status fields for USB stack, open attempts, successes, failures, disconnects.
- Preserved direct rusEFI USB protocol client, signature matching, SPIFFS INI cache and read-only default.
- Full ESP-IDF build and STM32 hardware test not available in this environment.
