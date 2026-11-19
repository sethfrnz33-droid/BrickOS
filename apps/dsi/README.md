# Brick Tuner Suite — DSi edition

**Current release: [v2.2 — 3DS-style workspace, 2/3/4 gauges, bitfield tuning, audio diagnostics](README_V22.md).**

This `DSi/` folder is a separate Nintendo DSi/DS build of Brick Tuner Suite. It coexists with the current **3DS** project: do **not** overwrite the root 3DS source, shared Makefile, or actively developed BrickOS ESP32 firmware.

### Build in devkitPro MSYS2

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```

The compiled DSi ROM is `DSi/Brick-Tuner-Suite-DSi.nds`.

### Important: install audio on the physical SD card

Copy `DSi/SD_Files/BrickECU/sfx/` to **`BrickECU/sfx/` on the DSi SD card**. All `00.pcm8` through `13.pcm8` files must be present. The `.nds` does not embed these sounds. The Soundboard now shows a specific failure message if audio assets are unavailable.

For control mappings, changes, limitations, and full test notes see [README_V22.md](README_V22.md). Older changes are documented in [README_V21.md](README_V21.md), [README_V20.md](README_V20.md), and [HISTORY_V01_TO_V18.md](HISTORY_V01_TO_V18.md).
