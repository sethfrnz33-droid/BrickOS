# BrickOS v3.4 — ECU SD INI import (hardware experimental)

Starts CDC host first and registers a separate USB MSC host client after CDC initialization.
The MSC component is patched during CMake configure to select **LUN1** on rusEFI composite USB storage (SD card, not embedded INI drive).
A background task waits for a live ECU signature then mounts /ecusd read-only in application behavior (format_if_mount_failed=false and no writes) and searches for any `*.ini` in SD root, `/rusEFI`, or `/tunerstudio`.
Files up to 1,200,000 bytes including the supplied `rusefi_proteus_f7.ini` are supported. An exact signature match is still required.
The scanner status and candidate filenames appear at `/api/status`: `ecu_sd_ini_state`, `ecu_sd_ini_attempts`, `ecu_sd_ini_candidates`, `ecu_sd_ini_last_file`, `ecu_sd_ini_last_error`, `ecu_sd_ini_imports`.
The `brick` Wi-Fi AP remains AP-only. No internet downloads.
IMPORTANT: Hardware behavior is unverified; MSC initialization/mounting may still fail if the ECU does not expose SD LUN1 during logging. The file picker is independent from auto-import.
