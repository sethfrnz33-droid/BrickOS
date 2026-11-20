BrickOS v3.4 GCC build fix

Fixed -Werror=format-truncation in main/usb_sd_ini.c at the SD candidate filename status copy. The stored display name is bounded and NUL-terminated. No changes to USB, Wi-Fi, SD probing, or INI validation.

Local syntax and JS tests passed. ESP-IDF 5.3.1 complete build not available in this environment.
