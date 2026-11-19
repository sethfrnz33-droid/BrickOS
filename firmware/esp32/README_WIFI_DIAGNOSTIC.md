# BrickOS v2.3 – Wi-Fi stability diagnostic

- Normal operation is Wi-Fi **AP-only** (`brick`, open, `192.168.4.1`), avoiding unnecessary STA mode.
- The USB host startup is deferred for 8 seconds after the AP starts, helping isolate the source of any disconnections. Native USB tuning is retained.
- `GET /api/status` includes `boot_counter`, `reset_reason`, `uptime_ms`, and `wifi_last_disconnect_reason`.
- GPIO17 intentionally blinks as a heartbeat; it **does not by itself prove rebooting**. GPIO18 tracks the phone's association.
- Downloading an INI via upstream Wi-Fi switches temporarily to AP+STA and restores AP-only afterward. Temporary AP channel changes may occur during download.

**Test with STM32 disconnected**: After connecting to `brick`, open http://192.168.4.1/api/status twice at least 10 seconds apart. If `boot_counter` increments or `uptime_ms` decreases, the ESP rebooted. If neither happens but GPIO18 goes off, the phone disassociated; use `wifi_last_disconnect_reason` to distinguish causes. The first 8 seconds have no USB manager running; afterward USB discovery starts.

This firmware is a diagnostic change, **not** proof the root cause is fixed or that the USB CDC/ECU connection works. The complete ESP-IDF build requires verification on the user's toolchain.
