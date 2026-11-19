# BrickOS v2.7 — ECU SD INI import (experimental)

**Target:** ESP32-S3 / ESP-IDF v5.3.1. **Wi-Fi:** open AP `brick`, `http://192.168.4.1/`. **Tuning:** native STM32 USB CDC; no CAN tuning. **No internet/network STA mode.** GPIO17 heartbeat and GPIO18 Wi-Fi association LEDs are active high.

## What this adds

rusEFI's USB mass-storage firmware exposes an embedded INI drive as **LUN0** and its removable SD card as **LUN1**, when USB MSD support and SD host ownership are enabled on the installed STM32 firmware. Stock Espressif `usb_host_msc` v1.1.4 always selects LUN0, so the project pins that component and changes its command-wrapper LUN to **1 during CMake configure**. The USB MSC class client is separate from the known-working CDC tuner interface.

After the ECU's USB signature is verified, the background reader attempts to mount LUN1 **without formatting**, scans for a **plain** `*.ini` file in the SD root, `/rusEFI`, or `/tunerstudio`, verifies the `[MegaTune] signature` exactly against the live ECU signature, copies a matching file into `/logs/ecu.ini` in ESP SPIFFS, then unmounts the SD. It never writes to or formats the ECU SD. Failed attempts are spaced 30 seconds apart; the USB tuning interface continues to work if the card or MSC interface isn't available. Internet downloads are disabled.

Place your **generated and firmware-matched `rusefi.ini` as an ordinary uncompressed text file in the SD card root**. Do not rename a `.7z` archive to `.ini`. The ESP32 cache accepts files up to 750,000 bytes; larger INIs are rejected without overwriting the previous cache.

## What the status page reports

`http://192.168.4.1/api/status`: `ecu_sd_ini_state`, `ecu_sd_ini_attempts`, `ecu_sd_ini_imports`, `ecu_signature`, `ini_cached`. The site's Connection page also displays the SD-import status and automatically loads a newly cached matching INI.

If the SD does not appear, verify whether the actual STM32 firmware build exposes USB Mass Storage and has detached the SD card from ECU logging. CDC USB tuning working is **not proof of an MSC interface**. This version does not command the ECU to eject its SD logging filesystem, and never attempts SD writes. If your ECU's USB firmware exposes only CDC, importing over MSC is impossible without another ECU-side file-transfer mechanism or an ECU firmware change. Manual browser INI upload remains available.

## Build

Extract the ZIP into its single `BrickOS_v27` folder. Use ESP-IDF 5.3.1. `idf.py set-target esp32s3` and `idf.py build flash`. The CMake configure stage downloads pinned `espressif/usb_host_msc==1.1.4` and selects LUN1 in its managed SCSI/BOT component. If the component source/API changes, CMake **fails closed** rather than silently mounting LUN0.

**Testing:** native host syntax checks and JS interface tests passed. A complete ESP-IDF cross-build and physical MSC LUN1 mount have **not** been performed in this environment; the feature must be validated on your ECU. Preserve your working v2.6 as rollback firmware if this experimental MSC component interferes with USB CDC.
