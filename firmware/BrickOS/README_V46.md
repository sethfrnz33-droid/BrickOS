# BrickOS v4.6: stop-only rusEFI simulator integration

- Added `POST /api/stim/stop`, using the fixed rusEFI **disable internal trigger simulation** command, not an arbitrary command pass-through.
- JSON response includes `command_acknowledged: true` and `running_state_verified: false`; an acknowledgement is not evidence of physical isolation.
- `/api/status` exposes explicit start/stop support and reports the running state as `UNKNOWN`.
- Remote internal trigger **start is not implemented**, because an unverified vehicle installation may energize ignition/injection hardware.
- Normal calibration workflow and existing 100 Hz sampling settings are unchanged.
- This release requires an ESP-IDF rebuild and flashing. Physical hardware and simulator state verification have **not** been performed.
