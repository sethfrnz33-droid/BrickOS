# BrickOS v4.0 — ECU calibration writes enabled

v4.0 fixes the two actual firmware-level write blockers in v3.9:

1. The calibration write feature is **ON by default** in both `main/Kconfig.projbuild` (`default y`) and `sdkconfig.defaults` (`CONFIG_BRICK_ALLOW_CAL_WRITES=y`). There is no unassigned physical GPIO jumper prerequisite.
2. The real `/api/ts` command allowlist now **includes `B`**, the flash-burn command. v3.9 accidentally rejected this command even if the firmware write feature was configured.

## In the editor

- Connect the ECU and load a generated INI whose signature matches the live rusEFI firmware.
- Edit map cells, X/Y axes, or INI-driven calibration fields. Edits remain staged locally.
- With a confirmed **stopped engine on a stationary test bench**, press **Verify & write / burn page** or **Verify & write all staged** and confirm the action.
- BrickOS **automatically reads and initiates a download of a complete backup of each affected ECU page** before transmitting writes. Check that your browser permits and saves the backup download.
- Before writing, BrickOS rereads all changed regions and refuses to transmit if the ECU's original values have changed. It then automatically arms a 60-second protocol session bound to the live ECU signature.
- It writes contiguous byte changes using `C` and verifies them with `R`. For INI pages declaring `B%2i`, it sends **one `B` per page**, then verifies again. Pages with no burn command are RAM-only and are labeled as nonpersistent.
- A fresh zero-RPM telemetry sample and an independent USB-side RPM check before each page are required. The write authorization is disarmed after the apply operation ends, whether successful or not.

No separate Arm button, physical jumper, or manual firmware configuration is required in a fresh build of this project.

## Limitations and security

- **Test the first write on a disconnected/stationary bench ECU.** Real hardware write/flash persistence has not been verified in this environment; integration tests use a simulated ECU.
- The default `brick` access point is **open, without client authentication**. During a write's short authorization interval, API access on that Wi-Fi network is not cryptographically authenticated; do not treat it as secure for unattended or public use. The build still requires UI confirmation, exact signature and a short arming interval, but these are not a substitute for Wi-Fi authentication.
- A successful immediate readback alone does not prove flash persistence after a power cycle or that changed calibration values are mechanically appropriate. Some settings may require a deliberate power cycle.
- This build does not claim full feature parity with TunerStudio, nor measured 100 Hz performance under physical write/load conditions.
- This package includes the full ESP32-S3 ESP-IDF project folder, sources, regenerated web assets, tests, and earlier release notes for history.

## Validation

- `python tests/test_v40_write_config.py` validates actual ESP32 C command routing for `C` and `B`, enabled-by-default build flags and stopped-engine/client checks.
- `node --test tests/*.test.mjs` tests INI interpretation and write plans.
- `python tests/v39_browser_smoke.py` exercises **the actual v4.0 web application** with a synthetic ECU, including automatic backup download, automatic arm, `C` writes, one `B` burn, readback, and stale-write refusal.
- `bash tests/run_host_syntax.sh` and `bash tests/run_assets_host.sh` validate host C syntax and web asset consistency.
- A complete ESP-IDF firmware build and physical ECU test were **not performed** here.
