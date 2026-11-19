# v4.1 host regression verification

19 `tests/test_v*.py` scripts passed individually on the host. Key new native checks:
- Project uses a real CGFX extended-banner build path with a source GLB, no flat 2D substitute.
- Real U16 calibration cells spanning 100..175 result in non-flat projected heights, and at least five colours appear.
- At full 3D slider, the same map point has visibly different eye projections.
- Truly constant ECU Z values display a flat-table warning.
- Legacy INI alias/map resolution, write checks and gauge de-duplication remain covered.

Caveat: tests run with host draw stubs, not devkitARM, a 3DS emulator, 3DS hardware or a real ECU.
The CGFX and CIA were not generated here; `build-cia.sh` must complete on the user's toolchain.
