# BrickOS v2.30 — iPhone INI import feedback

- Retains the proven AP-only STM32 USB CDC transport and experimental read-only MSC LUN1 SD import.
- File picker accepts INI/text formats without forcing the basename `rusefi.ini`; avoids unrestricted photo/camera options where iOS honors `accept`.
- Persistent inline status reports reading, parsing, signature validation, upload percentage, ESP32 confirmation, and actionable failures.
- File upload has a 60-second timeout, reports HTTP error details, and verifies the SPIFFS content by reading it back.
- Server size limit remains 750000 bytes; exact ECU signature matching remains mandatory for tuning safety. SD scanner already allowed arbitrary `.ini` basenames and now reports no-file vs rejected-file separately.
- Browser and host tests are local only; full ESP-IDF build and iOS/ECU hardware tests require physical devices.
