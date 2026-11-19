# Brick Tuner Suite v3.4.1 — host build-fix checks

All 13 previous Python source-contract tests plus `tests/native_mock_write.py`, all 8 native C unit tests, and the added `tests/test_v34_build_fix.py` passed on this host. The added test checks that leftover game sources cannot enter the link and that bounded string copies preserve guard bytes and terminate correctly.

`source/main.c` also compiled to a host object with GCC 14.2, using the supplied stub headers (no error), with **no** `-Wformat-truncation` or `-Warray-bounds` warnings under `-O2 -Wall -Wextra`. The host compile still raises expected unimplemented 3DS-API-stub diagnostics and pre-existing unrelated warnings.

**Not tested:** devkitARM 16.1, actual `tex3ds` output, `.3dsx` or `.cia` linking, 3DS hardware, Wi-Fi or ECU writes. Real console build and runtime verification must be done in the configured Windows devkitPro/MSYS2 environment.
