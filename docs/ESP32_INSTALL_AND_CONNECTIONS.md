# ESP32-S3: setup, flash, wiring, and diagnostics

Source of truth: **user-supplied BrickOS v4.8**. Verified by inspection of `main/main.c`, `main/vcp_tcp_gateway.cpp`, `main/ini_cache.c`, `main/usb_sd_ini.c`, `main/Kconfig.projbuild`, `sdkconfig.defaults`, and its ESP-IDF CMake files (2026-11-19). Physical board wiring beyond the SoC pins is **board-dependent and not established by this source**.

## Requirements

- ESP32-**S3** development board with at least **4 MB flash**, a **USB-OTG host connection** accessible to the ECU, and a separate programming/debug interface or a way to switch from programming mode to host mode.
- Verified, protected 5 V host **VBUS** power source, with current limiting; never connect VBUS directly to a 3.3 V GPIO.
- A rusEFI ECU exposing a **USB CDC** interface for tuning; optional **USB Mass Storage** with SD as LUN1 for automatic INI import.
- ESP-IDF **v5.3.1** environment (the release's documented target) plus Python; USB component dependencies are fetched via `main/idf_component.yml` during the build.
- USB data cable between **ESP32-S3 host USB** and **ECU USB device**. A separate cable/port between computer and ESP32 for programming/monitoring.
- A matching rusEFI INI, supplied via the web UI or (where supported) ECU SD import. The source's limit is **1,200,000 bytes**.

## Logical connections

```text
Computer ── programming/debug USB ── ESP32-S3 [programming interface]
                                        │
                                        ├── 2.4 GHz Wi-Fi AP "brick" (open)
                                        │       ├── 3DS client, later
                                        │       └── PC/phone browser 192.168.4.1
                                        │
                                        └── USB-OTG HOST (GPIO20 D+, GPIO19 D−)
                                                │ data + ground + protected 5 V VBUS
                                                ▼
                                         rusEFI ECU USB device
                                          (CDC, optional MSC)
```

**No CAN connection is used by this uploaded v4.8 firmware.** USB CDC is the active TunerStudio/ECU transport, and the firmware presents TCP port `2000` on its own Wi-Fi AP.

The **ESP32-S3 silicon** exposes USB D+ on GPIO20 and D− on GPIO19. Whether these pins are routed to a connector, require an OTG adapter/mux or need VBUS switching depends on the **specific development board**. USB VBUS needs a protected 5 V supply; do not assume a USB connector automatically provides host power. GPIO17 and GPIO18 are **active-high status LED outputs** in the source (heartbeat and Wi-Fi client, respectively), but their existence on the board is optional. Connecting LEDs requires correct series resistors and a compatible board circuit.

Optional **ESP-attached SPI SD** for log storage is **disabled by default** (`CONFIG_BRICK_SD_SPI=n`): CS=GPIO10, MOSI=GPIO11, SCLK=GPIO12, MISO=GPIO13. This is separate from rusEFI's own SD card, which can be accessed through ECU USB MSC LUN1 when exposed. Use only a suitable **3.3 V** SD interface; GPIO numbers may conflict with specific boards. No SD wiring is required for the default SPIFFS logging arrangement.

## Install firmware (Windows)

1. Install **Espressif ESP-IDF v5.3.1** with the ESP32-S3 toolchain; launch the ESP-IDF PowerShell/command environment, not an ordinary terminal lacking `idf.py`.
2. Download the repository and change into `firmware\esp32` (or extract the complete release ZIP and change to `BrickOS_v48`).
3. Connect the ESP32-S3 **programming interface** to the computer, and confirm its COM port using Device Manager. Do not guess a serial port.
4. Regenerate the built-in browser UI and configure/build:

```powershell
python tools/generate_web_assets.py
idf.py set-target esp32s3
idf.py build
```

5. After a successful build, flash the **ESP32-S3** (example port only):

```powershell
idf.py -p COM7 flash monitor
```

6. Restart the ESP32-S3 using the board's specified boot mechanism. Join Wi-Fi `brick` (no password); open `http://192.168.4.1/api/status`. Check `brick_os_build` reports **`v4.8`**; check `usb_stack_started`, `usb_device_ready`, `ecu_session_connected`, `ecu_signature`, and `ini_cached` separately.
7. With hardware appropriately configured for **isolated bench testing**, connect the ECU's USB CDC **device** port to the ESP32's correctly powered USB **host** port. Verify detection and the ECU identity in `/api/status`. If the ECU exposes no compatible USB CDC function, the default firmware cannot communicate with it; CAN wiring alone will not fix that.
8. Supply the **firmware-matched INI** through the browser upload, or place an ordinary `.ini` text file on the ECU SD root, `/rusEFI`, or `/tunerstudio` if the ECU exposes MSC LUN1. The ESP will only import a matching definition and will not format the ECU SD.

`idf.py flash` automatically builds if necessary, but a separate `idf.py build` is useful for checking dependency and linking errors before flashing. Generated `main/web_assets.c` must match `web/*`; the supplied release does match. The build's CMake intentionally pins `espressif/usb_host_msc` 1.1.4 and patches an LUN selector from 0 to 1; if the upstream source layout differs, configuration fails instead of silently selecting the wrong LUN.

## Common failures

| Symptom | Check |
|---|---|
| `idf.py` unavailable | Use Espressif ESP-IDF command environment with the selected toolchain |
| Wrong chip / target | ESP32-S3 is required; do not build for classic ESP32, S2 or ECU STM32 |
| USB never becomes ready | Actual USB-host mode, OTG D+/D− routing, protected VBUS power, USB cable and ECU CDC availability |
| Wi-Fi absent | ESP boot logs, programming port and AP setup; SSID is `brick`, not a phone hotspot |
| Wi-Fi connected but ECU disconnected | These are separate conditions; inspect `/api/status` ECU/USB fields |
| INI rejected | Compare the full ECU signature to the definition, check type, file size (≤1.2 MB) and cache state |
| SD import never succeeds | ECU must expose MSC **LUN1** with readable SD filesystem; CDC alone is insufficient |
| CMake fails on MSC LUN patch | Pin/downloaded component version mismatch; do not remove the guard without validating the component |
| Browser UI stale after editing web | Re-run `tools/generate_web_assets.py`, rebuild and reflash; refresh browser cache |

## Security and isolation

The current SSID `brick` is **open** and the browser interface includes calibration operations. Treat it as an isolated laboratory setup, not an authenticated public-network tuning product. Do not attach a live engine, fuel injectors, ignition coils or vehicle actuators for initial validation. The optional internal trigger simulator is a separate bench-only feature with limitations in `README_V47.md` and `README_V48.md`; an ECU acknowledgement does not prove physical output isolation. These instructions cover **ESP software installation**, not permission to operate or modify an engine on public roads.

## Reference documentation

- [Espressif ESP-IDF v5.3.1 Windows installation](https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32s3/get-started/windows-setup.html)
- [ESP32-S3 USB Host API](https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32s3/api-reference/peripherals/usb_host.html)
- [ESP32-S3 USB PHY pins and 5 V bus requirements](https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32s3/api-reference/peripherals/usb_device.html)
