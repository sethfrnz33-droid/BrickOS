# BrickOS v3.2 — USB reconnect/reboot isolation

The confirmed v2.6 USB CDC connection is restored as priority. V3.1 started
an experimental MSC host client before the CDC client; v3.2 does not install
MSC and does not create its SD scanner task. This is a **stability isolation
build**, not a completed SD-autoload implementation.

- Open Wi-Fi AP `brick` unchanged, no upstream STA or download code.
- Native STM32 USB CDC ECU signature auto-handshake unchanged.
- The 1.2 MB manual generated INI import fixes from v3.1 preserved.
- SPIFFS cached files and parser preserved.
- `/api/status` now states when USB SD auto-import is disabled.
- SD card contents are NEVER mounted or written by this firmware.

## Verify
1. Flash, connect phone and leave ECU connected for 60 seconds.
2. Revisit `http://192.168.4.1/api/status` twice and confirm `boot_counter`
   remains constant and `uptime_ms` increases.
3. Confirm `ecu_session_connected` stays true and phone remains associated.
4. Upload the firmware-matched .ini manually if needed.

The hardware reboot root cause is not established until those checks pass.
This build intentionally disables USB SD auto-import so we can determine if
MSC driver initialization caused the repeat panic.
