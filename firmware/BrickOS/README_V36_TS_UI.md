# BrickOS v3.6 – TunerStudio-style UI rebuild

Starting point: hardware-verified v3.4 SD + USB support, via v3.5 source. The firmware connection and storage logic is unchanged.

## UI changes
- Classic light desktop tuning style, inspired by ECU tuning applications rather than the prior neon dashboard.
- Actual rusEFI-generated [Menu] hierarchy, preserved in original INI order, including groupMenu/groupChildMenu entries.
- Each selected [Menu] item resolves [UserDefined] dialog panel references and [TableEditor] definitions to the original calibration fields and correct page offsets.
- Tables and settings open the pre-existing working read/visualize editor. Existing write safety gates remain unchanged.
- Unresolved conditional or unsupported dialog fields are shown as not supported, not substituted with arbitrary values.
- Desktop sidebar with expandable system tree. Mobile devices have a compact scrollable tuning tree.
- Manual field library, diagnostics, AP-only Wi-Fi, live gauges, SD-card INI imports and USB are retained.

## Validation
- Real user-provided Proteus F7 generated INI test fixture in tests/fixtures (used only for tests, not compiled into ESP32 image).
- 17 Node tests passed; Chrome browser rendered the VE screen without JavaScript errors; web asset C host and C syntax checks passed.
- Full ESP-IDF 5.3.1 build and physical ECU/UI testing are not available here.
- Firmware files byte identical to v3.5: main.c, ini_cache.c, protocol.c, usb_sd_ini.c, vcp_tcp_gateway.cpp, storage.c, idf_component.yml, main/CMakeLists.txt, partitions.csv.

Unzip into the one BrickOS_v36 directory, then use idf.py build flash monitor with the existing ESP-IDF 5.3.1 setup.
