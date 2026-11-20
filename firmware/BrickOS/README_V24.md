# BrickOS v2.4 — Native USB ECU session auto-retry

* USB host starts 1 second after boot, not 8 seconds. USB connection manager continuously retries CDC detection even if the browser never connects.
* Independent native USB TunerStudio `S` handshake task retries once per second after a USB CDC port is ready. Reads the signature directly from STM32, validates `rusEFI ` prefix, and retains it for that USB attachment only.
* Disconnect clears stored signature; reconnect triggers a fresh ECU signature verification.
* `/api/status`: `ecu_signature`, `ecu_session_connected`, `ecu_handshake_attempts`, `ecu_handshake_successes`, `ecu_handshake_state`, and existing USB open counters.
* Web app consumes a verified ESP-side signature and loads matching INI; manual Reconnect still supported.
* Wi-Fi AP-only `brick`, active-high GPIO17/18 LEDs, no CAN tuning.

This code is not verified against physical USB hardware or cross-compiled with ESP-IDF; test build on IDF v5.3.1.
