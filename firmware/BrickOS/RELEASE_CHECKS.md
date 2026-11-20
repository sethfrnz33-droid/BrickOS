# v4.4 release checks

Confirmed from source: 
- Real `/api/backup` reads the page via native rusEFI `R` requests and saves it to ESP32 attached SD or SPIFFS, then verifies stored length and CRC32 before acknowledging.
- The browser no longer depends on Safari localStorage for calibration backups. Manual binary download works after page refresh.
- Writes retain direct zero-RPM bench checks, exact ECU signature, automatic short-lived arming, data-conflict detection, `C` write readback, `B` flash burn and post-burn verification.
- `/api/status` reports `brick_os_build=v4.4` and backup storage health.

Automated checks completed: 26 JS tests, mocked browser calibration transaction including failed-storage and Safari localStorage error cases, ESP32 C host syntax checks, native backup CRC vector test.

**Not verified:** ESP-IDF firmware build, flash install, ESP32 storage mount capacity on the device, live rusEFI USB write/burn and ECU persistence on actual hardware.

Build with the existing ESP-IDF toolchain and flash the **ESP32-S3**, not the STM32 ECU. Confirm `/api/status` shows v4.4.
