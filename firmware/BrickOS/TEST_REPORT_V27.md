# v2.7 test scope

- C syntax checks: main.c, storage.c, usb_sd_ini.c, ini_cache.c using host stubs.
- Node interface and signature tests: 12 passed.
- Web static assets generated and checked.
- Build-source guard: pinned USB MSC driver 1.1.4; LUN0->LUN1 patch fails closed if source changes.
- READ-ONLY: FAT mount is never formatted; files are only opened read-only, and cache import checks `[MegaTune] signature` before commit.
- Not verified: complete ESP-IDF build, CDC+MSC composite host coexistence, actual Proteus USB MSC availability, SD filesystem mount, full-size INI storage, UI behavior on hardware.
