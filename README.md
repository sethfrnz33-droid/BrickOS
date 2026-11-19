# BrickOS — complete Brick ECU software workspace

**One repository for the ESP32-S3 firmware, Nintendo 3DS Brick Tuner Suite software, and Guided Tuning research/prototypes.** It is not exclusively an autotuner repository.

| Location | Role | Current status |
|---|---|---|
| [`firmware/esp32/`](firmware/esp32/) | ESP32-S3 Wi-Fi/USB bridge and embedded browser tuner | **v4.8 source imported from the user's release ZIP**; build not yet verified on hardware |
| [`apps/3ds/`](apps/3ds/) | Brick Tuner Suite native Nintendo 3DS v6.3 source, assets, build scripts and tests | **Imported**; host tests 42/42 pass; native build/hardware not yet verified |
| [`apps/dsi/`](apps/dsi/) | Brick Tuner Suite Nintendo DSi v2.2 source, tests and SD sound assets | **Imported** from user's v2.2 ZIP; host tests pass, hardware not verified |
| [`prototype/`](prototype/) | Standalone Guided Tuning Python reference implementation | Existing read-only simulation, not deployable ESP firmware |
| [`docs/`](docs/) | Guided Tuning design + installation, connections and firmware architecture | Research, specifications and setup instructions |
| [`ROADMAP.md`](ROADMAP.md) | Cross-project engineering roadmap | Planning only |

## ESP32 installation and connections

Start with **[ESP32 installation and hardware connections](docs/ESP32_INSTALL_AND_CONNECTIONS.md)**, then **[firmware architecture](docs/ESP32_FIRMWARE_ARCHITECTURE.md)**. The uploaded firmware targets the **ESP32-S3** with **ESP-IDF 5.3.1**, communicates with rusEFI using native USB CDC (not CAN), and exposes Wi-Fi AP `brick` at `http://192.168.4.1/` with TCP port `2000`. Its optional GPIO, USB power and SD details are described in the installation guide.

### Build on ESP-IDF 5.3.1

```powershell
cd firmware\esp32
python tools/generate_web_assets.py
idf.py set-target esp32s3
idf.py build
idf.py -p COM7 flash monitor
```

Use your verified ESP32 programming COM port in place of the **example** `COM7`. Do not flash this image to an STM32 ECU. For security, use the open Wi-Fi AP only on an isolated bench.

## Guided Tuning

- [Seth Full Tour — ten-stage sequence and DS-portable INI contract](docs/27-hpa-master-tour.md)

- [HP Academy public 10-step workflow comparison and recommended Guide refinements](docs/26-hp-academy-process-comparison.md)

### Current planning specifications (research/design, not implemented firmware)

- [ESP autotune runtime, analysis filters, ownership and controller lease](docs/19-autotune-runtime-and-controls.md)
- [Full INI-driven guided calibration journey and reasoning decisions](docs/20-guided-calibration-journey.md)
- [3DS two-screen Guided Tuning navigation and wireframes](docs/21-3ds-guided-layouts.md)
- [Phone browser Guided Tuning responsive layouts](docs/22-phone-guided-layouts.md)
- [Shared ESP/3DS/phone state contract and acceptance tests](docs/23-guided-tuning-protocol-and-acceptance.md)
- [Earlier UX specification](docs/10-user-experience.md) and [calibration proposal / review transaction model](docs/18-transaction-and-review.md)

Guided Tuning is a **separate, opt-in mode** designed to discover the active ECU/INI configuration, learn from valid telemetry, explain calibration dependencies, and make evidence-backed recommendations. It must never impede manual tuning or silently change ECU flash. The phone-first **read-only Guided Tuning** browser workspace is now integrated under `firmware/esp32/web/`, with [implementation notes](firmware/esp32/README_GUIDED_PHONE_V1.md). It uses the existing ESP USB live sampler and runs its learning decisions in the phone browser; the ESP-resident autonomous controller and automatic ECU changes remain unimplemented. The existing Python simulator under `prototype/` remains independent.

## Source provenance

The `firmware/esp32/` source is copied from `BrickOS-v48-TRIGGER-INI-FIX(2).zip` as supplied 2026-11-19; runtime source and embedded assets are unchanged. Its historical `README.md` (which referenced v4.2) is preserved as `README_LEGACY_V42.md` and replaced with the current setup README. The actual 3DS v6.3 and DSi v2.2 sources are now imported under `apps/`; integrating the proposed Guided Tuning controller into ESP firmware and client apps remains future work.


## Seth's Guide — phone-first, INI-dependent

**One guide data source, multiple future displays.** The phone now has a separately navigable **Ask Seth** experience (full ECU tour, targeted troubleshooting, field-by-field INI walkthrough), using the user's mascot portrait and gibberish audio. The story content lives in `firmware/esp32/web/guide-catalog.json` and is served by the ESP at **`GET /api/guide/v1/catalog`**. Seth is the lightly exasperated ECU-bound clone of the creator; he explains settings and diagnostics without interrupting Manual Tune.

- [Full authored Seth dialogue and branching choice script](docs/24-seth-complete-dialogue.md)
- [INI-bound, portable phone/DS dialogue contract and explicit limitations](docs/25-portable-ini-guide-contract.md)
- [Phone implementation, build, routes, media, and tests](firmware/esp32/README_SETH_GUIDE_V2.md)

**Compatibility:** No 3DS or DSi source or connection protocol was changed for this guide. A later ESP-hosted role/state service must produce resolved view packets for the DS, so changing the INI or Seth's lines does not require updating console code. The current phone resolves roles in the browser from the verified INI; this is a **read-only tutorial**, not ECU-resident automatic calibration or ignition tuning.
