# ECU SD INI import — multiple firmware definitions

The BrickOS USB MSC LUN1 importer now copies a valid ECU SD INI into the internal HTTP cache even when the connected ECU has a different firmware signature or the CDC signature is not available yet.

- Search locations: SD root, `rusEFI/`, and `tunerstudio/` on the ECU USB SD filesystem
- Prefer a single INI whose TunerStudio signature exactly matches the live ECU
- If there is no exact match, accept **one valid alternative** INI for **read-only download and UI parsing**
- If several unmatched INIs are present, stop with a clear ambiguity error; do not guess an ECU definition
- Accept the same structural checks as upload: [Constants] and [OutputChannels], regular file, 100 bytes–1.2 MB
- Cache replacement happens only after validation and successful copy; the previous working cache is not intentionally discarded on a failed import
- No physical output commands or ECU calibration writes are newly enabled; exact-signature and session checks still apply

Diagnostics: GET `http://192.168.4.1/api/status` and inspect `ecu_sd_ini_state`, `ecu_sd_ini_attempts`, `ecu_sd_ini_candidates`, `ecu_sd_ini_last_error`, and `ini_cached`. GET `/api/ini/file` downloads the imported definition after a successful scan.

This changes `firmware/esp32` only. DSi and 3DS source code are unchanged.
