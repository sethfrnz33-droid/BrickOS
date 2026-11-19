# Brick Tuner Suite v3.4.1 — linker and warning fix

## Root cause

The v3.4 ZIP contains **only** `source/main.c` and `source/font_assets.S` as C/assembler translation units. The user's local build additionally compiled `pikmin_scene.c`, `garden_assets.S` and `instruments_assets.S` (left over from another project). Both `garden_assets.S` and `font_assets.S` export `dreamfont_start` / `dreamfont_end`, so the linker reported multiple definitions.

The Makefile now uses an explicit `main.o font_assets.o` source manifest instead of wildcard-compiling every `.c` / `.S` file in `source/`. This keeps unrelated assets from entering Brick Tuner Suite even when updating an existing directory. The unrelated files do not need to be deleted.

## Other fixes

- Removed an unused `selected_at` placeholder.
- Replaced warning-producing string copies with a bounded, NUL-terminating helper; overlong labels continue to truncate safely.
- Increased the instrument precision display buffer so any possible decimal integer fits.
- Replaced the UI-sound tag `strcpy` with the bounded helper.

## Build command (MSYS2)

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

**Important:** devkitARM, tex3ds, makerom.exe, and bannertool.exe are required for the complete CIA build. This ZIP contains source, not a prebuilt CIA.
