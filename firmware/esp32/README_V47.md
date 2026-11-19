# BrickOS v4.7 — guarded bench-only internal trigger simulator

Based on v4.6, preserving native USB, the tuning dashboard, calibration writes,
flash burn, one-tap burn, INI caching, storage backups, and readback checks.

## Genuine rusEFI interface

- `POST /api/stim/preflight` checks the live ECU signature against the cached
  rusEFI INI, then reads **fresh** ECU `RPMValue` and `VBatt` via USB `O` commands.
- Preconditions: matching ECU/INI; `VBatt < 1.0 V` (unpowered bench PCB);
  engine `RPMValue == 0`, unless this BrickOS session already received an
  acknowledgement from the same ECU after sending internal simulator Start.
- Preflight returns a **single-use 15-second challenge**.
- `POST /api/stim/start` requires that challenge, the same ECU signature and a
  special confirmation string sent by Brick Tuner Suite after L+R. It repeats
  both ECU measurements before sending the fixed `Z 00 14 00 0d` rusEFI
  internal simulator Enable request. It does not expose arbitrary X14 commands.
- Repeated Start in an already-active Bridge session renews the timer without
  retransmitting Enable. Synthetic RPM does not trigger the initial-RPM lockout.
- `POST /api/stim/stop` sends the fixed `Z 00 14 00 0f` Disable command,
  without an RPM or challenge precondition.
- The 60-second watchdog tries to stop on timeout or after 5 seconds without
  any Wi-Fi clients; it retries failed stops while the USB connection exists.
  Disconnection means Stop cannot be guaranteed; the session state is cleared.

**Important:** The software reads ECU-reported voltage, not a physically
independent safety circuit. Even a successful ECU acknowledgement does **not**
prove the simulator has started/stopped or that an ECU output is electrically
isolated. Use only an unpowered, disconnected bench PCB; no actuators attached.
No proof of physical hardware function has been obtained in this environment.

## Version and API

`/api/status`: `brick_os_build: v4.7`, `stim_remote_start_supported` becomes
true only when the automatic-stop watchdog starts, and `stim_running_state`
shows `START_ACKNOWLEDGED` or `NOT_STARTED_BY_BRICKOS` (not authoritative ECU
output state). The cached INI must match the live firmware signature exactly.

## Validation

- Host C syntax tests for `main.c`, `ini_cache.c`, telemetry and storage.
- Native host test for the exact generated rusEFI INI RPM and battery channels.
- Native safety policy tests covering initial RPM, synthetic RPM, missing
  samples and battery voltage.
- 3DS socket API mock for preflight/Start/Stop and failed preflight.
- No ESP-IDF cross-compilation or physical ECU testing was possible here.
