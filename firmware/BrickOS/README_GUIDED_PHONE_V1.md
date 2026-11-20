# Guided Tuning — phone-first browser implementation

**Release status:** browser-side, read-only diagnostic and learning preview built on the imported ESP v4.8 USB telemetry transport. This is **not** the final ESP-resident automatic calibration controller.

## New browser workspace

The browser's navigation now includes **Guide**, independently of **Tune**, Dashboard, Gauges, Logs and ECU. All current 3DS and DSi source and connection protocols remain unchanged.

The Guide has five sections:

- **Next:** top evidence-backed priority and missing prerequisites.
- **Systems:** twelve lifecycle stages, including identity, sensors, sync, first-start observation, warmup, idle, fueling, STFT/LTFT, transients, ignition review, advanced configuration and final verification.
- **Evidence:** measured channels, rejection reasons, optional read-only session learning, configurable filters and an exploratory 8×8 RPM/load coverage grid.
- **INI Atlas:** inventory of decoded calibration fields, with links to the original manual ECU editor only when an actual INI menu resolves that field.
- **History:** local session audit messages and JSON review export.

The Guide reads **verified ECU-matched INI definitions** via the existing `core.mjs` parser and consumes the shared `/api/live` telemetry sampler. Units/scaling are decoded using field metadata, not hardcoded raw offsets. It distinguishes `Gego` (100-neutral) from per-bank STFT/LTFT and rejects evidence masked by non-neutral corrections, invalid lambda, unstable throttle/load/RPM, or missing channels. Evidence buckets are **not** ECU VE cells and must not be portrayed as approved tuning changes.

## Boundaries and source truth

**No new ECU write endpoint, automatic spark advance, RAM application, or flash save was added.** Learning and eligibility accounting execute in the **phone browser**, and therefore stop if the browser disconnects or closes. The ESP already collects raw ECU telemetry independently from browser frames, but a future firmware-side expert/state engine is required for continuous learning independent of Wi-Fi UI speed.

The Configuration Atlas accounts for what the existing parser can decode; it does not yet support all INI expressions, active-conditional branches, or full semantic coverage. Engine safety, physical sensor condition and ignition MBT cannot be established from a telemetry summary.

The existing open Wi-Fi AP offers no secure identity for autonomous calibration authorization. Future write capabilities require a separate authenticated controller protocol and hardware validation.

## Build (ESP-IDF 5.3.1, ESP32-S3)

From `firmware/esp32`:

```sh
idf.py set-target esp32s3
idf.py build
idf.py -p YOUR_PORT flash monitor
```

`main/CMakeLists.txt` now runs the embedded web asset generator at configure time. To regenerate manually after web edits: `python tools/generate_web_assets.py`. This preserves the existing USB host and Wi-Fi configuration.

## Verification performed

- `node --test tests/*.test.mjs`: **33 tests passed** (including 7 new Guided Tuning tests).
- `bash tests/run_host_syntax.sh`: passed.
- `bash tests/run_assets_host.sh`: passed using regenerated assets.
- Static 390px mobile preview checked for horizontal overflow.

**Not performed:** ESP-IDF cross compilation, real ECU connections, 3DS testing, and bench validation of newly introduced UI. Do not equate host tests with safe engine tuning.

Refer to `docs/19-autotune-runtime-and-controls.md` through `docs/23-guided-tuning-protocol-and-acceptance.md` for the larger planned architecture.