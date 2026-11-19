# BrickOS v4.7 host test report

- Host `gcc -fsyntax-only` for main, native USB import, INI parser, storage and
  telemetry: PASS.
- Generated rusEFI INI actual parsed channels: RPM U16 offset 4, VBatt U16
  offset 36, exact live ECU signature required: PASS.
- Actual policy header: RPM 0 when stim off; nonzero RPM permitted only when
  stim already acknowledged for that ECU session; fresh measured VBatt <1 V
  always required; invalid or stale readings rejected: PASS.
- Web JS tests: 13/13 PASS.
- 3DS HTTP mock preflight/start/stop and preflight denial: PASS.

**Not run:** ESP-IDF full compilation, flashed ESP32 or physical rusEFI
hardware/outputs. `command_acknowledged` is not independent proof of pin state.
