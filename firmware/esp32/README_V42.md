# BrickOS v4.2 — Calibration writes no longer depend on old build configuration

## Root cause of the “unavailable in this firmware build” message

In v4.1 `web/app.js` explicitly stopped whenever `/api/status` returned `write_compiled=false` or `write_access_ready=false`. Both values came from the `CONFIG_BRICK_ALLOW_CAL_WRITES` ESP-IDF Kconfig option in `main/main.c`. Even though `sdkconfig.defaults` requested `y`, **an existing `sdkconfig` could persist an earlier `n`**. The host syntax check in v4.1 also forcibly defined the option as `1`, masking this problem.

## Fix

- The `CONFIG_BRICK_ALLOW_CAL_WRITES` switch is **completely removed** from the source and Kconfig. The rusEFI `C` write and `B` burn commands are included in every v4.2 firmware build, even if the local `sdkconfig` still has old flag values.
- `/api/status` now unconditionally reports `brick_os_build: "v4.2"`, `write_compiled: true`, and `write_access_ready: true` for this firmware.
- The browser asks the ESP32 for a fresh `/api/status` when Apply is clicked and reports which installed firmware is blocking writes, rather than a generic unexplained error.
- The editor's automatic temporary arming, live ECU signature check, direct stopped-engine RPM read, complete-page backup, pre-write conflict check, readback verification, and supported-page flash burn remain intact.
- `main/web_assets.c` is regenerated from `web/` so the ESP32 firmware actually serves the updated interface.

## You MUST flash the new ESP32 firmware

Unzipping a new project, or refreshing an old web page, **does not update the microcontroller**. The write-lock message is generated from the write capabilities of the firmware installed on the actual ESP32.

1. Open an ESP-IDF 5.3.1 environment and navigate to this **BrickOS_v42** directory.
2. Run `python tools/generate_web_assets.py` and `idf.py build`.
3. Flash the **ESP32-S3**, using the correct serial port for your hardware, via `idf.py -p <YOUR_PORT> flash`.
4. Reconnect to Wi-Fi `brick` and open `http://192.168.4.1/api/status` in the browser. Confirm all three: `"brick_os_build":"v4.2"`, `"write_compiled":true`, `"write_access_ready":true`.
5. Reload the normal UI at `http://192.168.4.1/` with a hard refresh if necessary.

Do not flash the STM32 ECU with this ESP32 firmware. Do not guess the serial port or target board; verify your ESP32-S3 connection first. This ZIP contains firmware **source**, not a built `.bin` image. Build and physical hardware write/persistence checks have not been performed here.

## Tests

`python tests/test_v42_write_config.py` verifies the build flag is absent from the firmware, runtime status and editor checks. `sh tests/run_host_syntax.sh`, `node --test tests/*.test.mjs`, and `python tests/v39_browser_smoke.py` run host/source and synthetic ECU checks (not hardware proof).
