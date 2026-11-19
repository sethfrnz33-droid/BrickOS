# BrickOS ESP Browser — Seth Guided Tuning V2

**Phone-first browser implementation; currently read-only.** The supplied pixel-art head, independently split jaw and uploaded gibberish voice are included in the firmware's embedded assets. Seth's lore is an occasionally grumpy, ECU-bound copy of the creator who genuinely explains what to do next.

## How to open

Build and flash the ESP32-S3 using the existing documented ESP-IDF procedure. Connect your phone to the ESP access point `brick`, open `http://192.168.4.1/`, then select **Guide**. The initial welcome presents **Take me through the whole ECU**, **I need help with something**, **Continue where I left off** and **See ECU evidence / full INI Atlas**. The guide works in explanatory mode with no ECU connected, but never invents live readings or usable editor links.

## Runtime and source files

- `web/guide-catalog.json`: versioned complete authored dialogue, target categories and role aliases.
- `web/guide-story.mjs`: branching dialogue evaluator, stale data handling, catalogue-supplied channel aliases, no-eval template bindings and INI-derived generic field pages.
- `web/guide-story-ui.mjs`: separate responsive phone UI.
- `web/guide-voice.mjs`: user-provided nonverbal recording with optional waveform-synchronized mouth animation, typewriter text, mute and skip.
- `web/guide-assets/*`: transparent head/jaw/mouth sprite images and transcoded MP3 from supplied assets.
- `web/app.js`: routes guided conversation separately from existing evidence and Manual Tune.
- `tools/generate_web_assets.py`: embeds all modules, JSON and media for offline hosting by ESP.
- `main/main.c`: **GET-only** `/api/guide/v1/catalog`, 3 JS module routes and 4 media routes. No new ECU write route.
- `tests/seth-guide.test.mjs`: catalogue, journey, INI, state and read-only regression tests.

## Portable DS interface

The current ESP can serve the same authored JSON to any HTTP client without console-side dialogue text. The future DS implementation should display a **server-produced view packet**, not reconstruct this logic itself. The ESP-side state/role resolver is a future project; the current phone implementation still evaluates the dialogue in the browser using the verified INI. See `docs/25-portable-ini-guide-contract.md` and the complete generated script `docs/24-seth-complete-dialogue.md`.

## Limitations

This release does **not** have autonomous ECU-resident learning, verified full INI semantics, ignition timing autotune, permission for writes, or a finished 3DS guide. User-supplied audio is converted to lower-bitrate MP3 to fit ESP flash; the actual recording remains the voice source. Face layers use the verified lip-seam geometry from the 3DS mascot project. Imported 3DS/DSi source and network interfaces remain unchanged.

## Build / test

```sh
cd firmware/esp32
python tools/generate_web_assets.py
node --test tests/*.test.mjs
bash tests/run_host_syntax.sh
bash tests/run_assets_host.sh
idf.py set-target esp32s3
idf.py build
idf.py -p YOUR_PORT flash monitor
```

Host tests are not ESP-IDF cross compilation or physical engine validation. Flash only onto a correctly configured ESP32-S3 board, not the ECU MCU. Check the correct partition sizes before shipping media-inclusive firmware.

## Full Tour update — catalogue v1.1.0

Seth's default Full Tour now follows the ten-stage sequence published in HP Academy's public Practical Standalone Tuning course outline, with explicit base-table discovery and physical-reference verification before the starting and idle chapters. This is a **read-only dialogue workflow**, not an operating procedure or ECU calibration algorithm. Quick Help, INI Atlas, the original manual editor, and existing DS network protocols remain unchanged.

The canonical versioned dialogue is `web/guide-catalog.json`; every fixed response is documented in `docs/24-seth-complete-dialogue.md`, and the stage ordering plus future DS packet fields in `docs/27-hpa-master-tour.md`. Run `python tools/generate_web_assets.py` after changing source assets, or let the existing ESP-IDF configure step regenerate them. Hardware validation remains outstanding.

## Full Tour chapter checks — dialogue catalogue 1.2.0

Each of the ten tour chapters offers three context-aware questions. Seth shows matched INI-channel evidence and explains missing inputs; users may report a review, say they do not know, request an explanation, inspect evidence or open a verified original INI menu item. Browser-local user reports never count as independent ECU validation. This update adds no ECU writes, ESP-resident autotuning, 3DS guide screens or DS connection changes. [Dialogue script](../../docs/28-seth-chapter-questions.md).
