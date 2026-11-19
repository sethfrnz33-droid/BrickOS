# Brick Tuner Suite v5.1 — Test Report

- **29/29 Python test scripts: PASS**, run in two batches to avoid command timeouts.
- Included new `tests/test_v51_donut_intro.py`: 16 unique transparent car frames, startup replacement, skip path, host compilation syntax.
- Included new `tests/test_v51_intro_fx.py`: smoke atlas alpha/levels, BRICK ECU title, correct build asset wiring.
- Legacy E46 diorama regression was updated to reflect the v5.0 title size and removal of the obsolete lean fallback; the E46 HOME Menu geometry/build script were not changed.
- ECU protocol mock: 6/6 scenarios passed (success, storage error, engine running, baseline mismatch, write rejection, flash rejection).
- **Limitations:** Host tests cannot verify the devkitARM build, tex3ds-generated sprite atlas, rendered animation speed, or real 3DS performance.
