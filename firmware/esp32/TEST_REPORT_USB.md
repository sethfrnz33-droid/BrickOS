# USB-only integration verification

- Source based on original user-supplied `working3wayhub_tcp_direct(1).zip` USB CDC ACM/VCP gateway; `main/vcp_tcp_gateway.cpp`, `tsdiag_parser.cpp`, and `idf_component.yml` preserved with removal of one stray comment terminator.
- BrickOS browser continues to use existing /api/ts endpoint, now calling CRC-framed rusEFI over local TCP port 2000 to the USB host bridge.
- External TunerStudio connects directly to 192.168.4.1:2000, same as working project. Sessions exclusive.
- All active CAN/TWAI GPIOs, ISO-TP machinery, CAN menuconfig, and CAN-based TCP/29001 bridge removed.
- `brick` open AP, INI SPIFFS cache, exact signature handling, logger, and existing web interface retained.
- `bash tests/run_host_syntax.sh`: PASS (main.c/storage.c)
- `bash tests/run_assets_host.sh`: PASS
- `node --test tests/core.test.mjs tests/ini_signature.test.mjs`: PASS 12/12
- Static USB integration checks: PASS
- Full ESP-IDF build: NOT AVAILABLE IN CONTAINER (ESP-IDF toolchain absent)
- USB hardware handshake: NOT YET VERIFIED ON DEVICE

**Security**: raw TCP port 2000 transports all TunerStudio commands without filtering because it duplicates the known-good transparent gateway. An open AP permits other nearby clients to access the ECU; secure the network before using it for tuning.
