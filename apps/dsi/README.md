# Brick Tuner Suite — DSi edition

**Current release: [v29 — working 2D curve links, Favorites, uniform 3D table cells, and INI status indicators](README_V29.md).**

This `DSi/` folder is a separate Nintendo DSi/DS build of Brick Tuner Suite. It coexists with the current **3DS** project: do **not** overwrite the root 3DS source, shared Makefile, or actively developed BrickOS ESP32 firmware.

### Build in devkitPro MSYS2

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```

The compiled DSi ROM is `DSi/Brick-Tuner-Suite-DSi.nds`.

### Important: install audio on the physical SD card

Copy `DSi/SD_Files/BrickECU/sfx/` to **`BrickECU/sfx/` on the DSi SD card**. All `00.pcm8` through `13.pcm8` files must be present. The `.nds` does not embed these sounds. The Soundboard now shows a specific failure message if audio assets are unavailable.

For control mappings, changes, limitations, and full test notes see [README_V24.md](README_V24.md) and [README_V23.md](README_V23.md). Older changes are documented in [README_V21.md](README_V21.md), [README_V20.md](README_V20.md), and [HISTORY_V01_TO_V18.md](HISTORY_V01_TO_V18.md).


**v25:** See [README_V25.md](README_V25.md) for map axes, trace, two-screen layout, contextual help, and START-key updates.

For the v26 native INI reference gauge, placement, and START/A confirmation update, see [README_V26.md](README_V26.md).

For the v27 nonblocking protected-write fix, see [README_V27.md](README_V27.md).

For v28 larger tuning font, accelerated nested folder navigation, and real DSi software-rendered 3D tables, see [README_V28.md](README_V28.md).

For v29 table link, Favorites, live indicator and noninteractive label/button UI fixes, see [README_V29.md](README_V29.md).
