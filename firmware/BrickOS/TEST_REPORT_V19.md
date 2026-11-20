# BrickOS v1.9 USB auto-connect patch

- USB manager now starts USB host/CDC enumeration at boot instead of waiting for a TCP client.
- Dedicated USB readiness exported to `/api/status` and displayed on the Connection screen.
- HTTP ECU queries wait up to 6 s for USB enumeration instead of sending into a disconnected gateway.
- Browser refuses a query while desktop TunerStudio holds the raw USB TCP bridge.
- Unchanged: SSID `brick`, IP 192.168.4.1, TCP port 2000, SPIFFS INI cache.

## Local validation
- Python regression for USB manager + readiness handling: PASS
- Existing host C syntax checks: PASS
- Web assets host check: PASS
- Node 12 interface + INI tests: PASS
- JavaScript syntax check: PASS

ESP-IDF 5.3.1 full build and physical USB enumeration are not verified in this environment.
