# BrickOS v4.8 — trigger preflight correction

The v4.7 trigger preflight used one generic error for missing INI,
USB RPM read failure, nonzero RPM, and battery voltage failure.
The UI consequently reported an INI requirement even when the actual
problem was an RPM or battery sample.

- Uses the current matching cached INI when available.
- If none is available, the **exact** rusEFI firmware build
  `rusEFI master.2026.11.09.proteus_f7.2735508644` can now use embedded
  RPM (U16 at offset 4, 1 RPM/LSB) and VBatt (U16 at offset 36, 0.001 V/LSB)
  output-channel descriptors from the included generated INI.
  This avoids requiring a second INI copy on ESP32 for that build.
- Other firmware versions remain supported using a matching INI. Unrecognized
  versions cannot safely use those fixed offsets, and must provide a
  matching definition. No offsets are guessed.
- Preflight now reports the exact failed check. RPM is measured fresh from
  ECU USB and must be zero for initial Start; measured VBatt must be below
  1 V as in v4.7. L+R authorization, the one-use challenge and Stop watchdog
  are unchanged.
- `/api/status` reports `brick_os_build: v4.8` and
  `stim_builtin_profile_available` for the known firmware version.

**Tests are source/host only.** No physical ECU or ESP-IDF cross-build
was available. Use alongside Brick Tuner Suite v2.5; the 3DS code does
not need changes for this server-side fix.
