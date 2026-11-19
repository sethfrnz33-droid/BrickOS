# BrickOS v2.5 — AP-only USB diagnostic build

- **Wi-Fi only AP**: SSID `brick`, open, `http://192.168.4.1/`. No STA netif, no AP+STA transitions, no INI downloads from the internet, no fetch API.
- **USB**: native ESP32-S3 host + direct rusEFI protocol. Host starts 1 second after AP startup; retries absent CDC devices once per second.
- **USB host task**: minimum nonzero 10 ms delay to avoid CPU0 starvation on an empty USB bus.
- **INI**: manual file upload and SPIFFS cached file still work, but all internet retrieval is disabled.
- **LEDs**: GPIO17 heartbeat, GPIO18 Wi-Fi client association, active high.
- **Diagnostic**: `/api/status` reports `boot_counter`, `uptime_ms`, USB state and handshake attempts.

NOTE: This is a USB/AP isolation build. No full ESP-IDF compile or physical hardware validation was possible in this environment.
