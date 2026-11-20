# BrickOS v2.6 — STM32 CDC-only USB stability candidate

- Wi-Fi AP-only `brick`, open, `192.168.4.1`; no STA, no internet fetch.
- Native STM32 USB CDC-ACM only (USB VID 0x0483, PID 0x5740, interface 1). Generic VCP driver discovery is disabled to avoid unnecessary probing on an absent ECU.
- Removed `usb_host_device_free_all()` from USB host events, eliminating a repeated host-device lifecycle operation during enumeration.
- One second after boot, start USB discovery and retry missing ECU devices. BrickOS still owns the rusEFI `S` handshake directly.
- Active-high GPIO17 heartbeat / GPIO18 Wi-Fi associated.
- `/api/status` displays USB open attempts and handshake attempts, uptime and reset reason.

NOTE: Not cross-compiled with ESP-IDF 5.3.1 or tested on physical ESP32-S3. If Wi-Fi still restarts, collect two `/api/status` snapshots so we can distinguish reboot versus phone reassociation.
