> **Current imported release: v6.3.** See [RELEASE_V63_3D_MAP_CAMERA_FIX.md](RELEASE_V63_3D_MAP_CAMERA_FIX.md) for camera changes and [TEST_FEATURE_STATUS.md](TEST_FEATURE_STATUS.md) for test-feature implementation status. The historical v3.6/v3.7 notes below document earlier layouts.

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
