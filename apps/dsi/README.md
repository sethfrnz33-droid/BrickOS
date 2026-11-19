# Brick Tuner Suite — DSi edition

**Status: DSi v2.1 source bundle prepared, not yet imported into this GitHub directory.** The complete DSi source archive is the additive `Brick-Tuner-Suite-DSi-v21-30Hz-Idle-Gauges-Full-Tuning.zip` distributed with the current ChatGPT release. Do not use any older v62 3DS snapshot to populate or replace the actively developed `apps/3ds/` source.

## DSi v2.1 integration requirements

- Preserve the live ESP32 BrickOS implementation in `firmware/esp32/` and preserve all 3DS files in `apps/3ds/`; **no force pushes or tree replacements**.
- Import the DSi archive's `BrickECU-3DS-Probe/DSi/` contents into this `apps/dsi/` directory additively. The archive contains `Makefile`, `source/` (including the new `dashboard.c/.h`), `tests/`, and 14 PCM8 sounds under `SD_Files/BrickECU/sfx/`.
- Build the DSi source using devkitPro MSYS2 `nds-dev`, e.g. `cd apps/dsi && make clean && make` once the source is imported. The existing local command remains `cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make`. **Do not modify the 3DS build scripts.**
- User-observed DSi v2.0: Wi-Fi connects through Unlaunch, INI loads from SD without re-parsing every startup, and telemetry is working.
- DSi v2.1 targets 30 **distinct** samples/sec (ESP supports `hz=50`), removes a duplicate VBlank wait, distinguishes repeated sample frames from failures, and provides six additional nonduplicate INI-driven lower-screen gauges after 10 seconds of inactivity. The actual hardware update rate remains unverified.
- The DSi tuning interface supports INI-defined maps, curves, scalar/array fields, nested dialogs, live map tracing, touchscreen numeric entry with local staged values, and explicitly confirmed verified bench-only writes with ECU/session/RPM/page/backup/readback checks. DSi logs and log analysis remain excluded. Not all 3DS advanced INI widgets are ported.
- Tests: existing HTTP/INI/sockets, HTTP 503, parser, axes, and protected-write regressions; new idle/duplicate-sequence, keypad/limits, nested dialogs, gauge-pref persistence. Native devkitARM compilation and real hardware behavior still require testing.

For protocol details see [DSI_PORT_AND_3DS_INTEGRATION](../../docs/DSI_PORT_AND_3DS_INTEGRATION.md), [DSI_HTTP_503_ANALYSIS](../../docs/DSI_HTTP_503_ANALYSIS.md), and [DSI_V20_SD_HTTP_RELEASE_NOTES](../../docs/DSI_V20_SD_HTTP_RELEASE_NOTES.md).

**This README does not claim the DSi source is present or compiled on GitHub yet.** Keep that distinction until an additive source import is committed and tested.
