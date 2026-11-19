# BrickOS v4.1 — Write-path reliability corrections

This release supersedes v4.0. It removes the **false write lockout** caused by requiring an already-running dashboard poll to be <1.5 seconds old and equal to zero RPM. Applying a change now queries RPM directly from the ECU and rechecks immediately before each page write. The engine-stopped check remains for bench safety.

Changes:

- `main/main.c` now explicitly includes the ESP-IDF-generated `sdkconfig.h`, ensuring the compiled write flag is actually visible. `write_compiled=true` by default in a **fresh** ESP-IDF configuration; no physical jumper is required. Existing `sdkconfig` settings can still override `sdkconfig.defaults` — inspect the runtime `/api/status` rather than assuming what was flashed.
- The UI clearly distinguishes **write support enabled** from **write authorization active**. Authorization is automatically opened during Apply and closed afterward; it does not need to be manually armed.
- Prioritizes calibration `R`, `C`, and `B` commands over background gauge USB polling for 1.8 seconds after each such request, reducing avoidable network-busy errors during page reads and flash operations.
- Automatically retries only **idempotent** `R`/`O` reads after transient USB/network timeouts, never blindly reissues `C` writes or `B` flash burns after ambiguous responses.
- Extends the 60-second authorization during successful write or burn commands so multi-page operations are not cut off mid-transaction. Disarms on UI completion or ECU USB disconnect.
- Runtime status now reports `brick_os_build: v4.1`. If the value is missing, you have not verified that this version is running on the ESP32.

## Bench write procedure

Use with a stopped engine on stationary, secured bench hardware. Connect the ECU, verify the generated INI matches the live signature, edit a value, and press **Verify & write / burn page**. BrickOS directly checks RPM, automatically downloads a backup if needed, checks source bytes, writes changed chunks, verifies readback, and burns the page only when the INI declares a burn command. It may be RAM-only on a page with no burn command. ECU firmware may require a power cycle for some settings; a successful readback is not proof of flash persistence.

## Building and checking actual hardware

`python tools/generate_web_assets.py`, then `idf.py build` and install the resulting firmware through your normal ESP-IDF workflow. The ZIP is **source**, not a flashed ESP32 image. An already-existing `sdkconfig` may retain `CONFIG_BRICK_ALLOW_CAL_WRITES=n`; check the actual ESP32 `/api/status` fields `brick_os_build` and `write_compiled` to determine which firmware/configuration is running.

Run `node --test tests/*.test.mjs`, `python tests/v39_browser_smoke.py`, `python tests/test_v40_write_config.py` and both host syntax and asset checks. Browser and host tests use a synthetic ECU; physical flash and persistence remain unverified.
