> **Current release: v6.5 — TunerStudio INI numeric conversion modes, limits, and read-only metadata.** See [RELEASE_V65_TUNERSTUDIO_EXPRESSIONS.md](RELEASE_V65_TUNERSTUDIO_EXPRESSIONS.md) and the previous [v6.4 mascot release](RELEASE_V64_MASCOT_AND_INI.md). Original 3DS hardware and CIA conversion are not verified.

> **Current release: v3.7.** See [RELEASE_V37_TUNERSTUDIO_TWO_SCREEN_TREE.md](RELEASE_V37_TUNERSTUDIO_TWO_SCREEN_TREE.md) for the ten-icon TunerStudio upper-screen tree, lower-screen live preview and DS settings cleanup.

# Brick Tuner Suite v3.6 — Six-gauge Home + Real Top-Screen Tuning Tree

**Start here:** [RELEASE_V36_SPLIT_SCREEN.md](RELEASE_V36_SPLIT_SCREEN.md) describes the redesigned two-screen navigation and controls, based on the supplied rusEFI/TunerStudio menu screenshots.

**Validation:** [TEST_REPORT_V36.md](TEST_REPORT_V36.md). **Previous editor controls:** [README_V35_CONTROLS.md](README_V35_CONTROLS.md).

The Home screen shows six real ECU channels across the upper screen with a left Wi-Fi / 3DS battery / actual receive-Hz strip. The lower screen has four workspaces: Tune, Analyze, Logging, DS Settings. Tune moves the INI-derived folder hierarchy to the upper screen, while the lower screen provides context, open/map actions, and RPM/CLT/AFR reference readouts.

Earlier release notes, feature roadmap and CSV feature tracker are retained. The build manifest fix remains documented in [BUILD_FIX_V34_1.md](BUILD_FIX_V34_1.md).

**Full MSYS2 build command:**

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

The ZIP contains the full source project in a `BrickECU-3DS-Probe/` folder, **not** a compiled or hardware-verified `.cia`.

## Conditional sound effects (v5.4)

All 11 supplied sound effects are embedded in the executable as 22.05 kHz mono PCM. Build/install the app normally; you do not need to convert MP3s, install WAVs, or copy a `sounds` folder onto the 3DS SD card. See `RELEASE_V54_EMBEDDED_CONDITIONAL_AUDIO.md` for details.
