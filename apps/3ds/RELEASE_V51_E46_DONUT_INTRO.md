# Brick Tuner Suite v5.1 — E46 Donut Startup

The old two-second text/status startup screen is removed.

New behavior:
- A brief 3.65-second donut sequence plays on the 3DS upper screen when the app opens.
- Sixteen transparent views of the user's actual low-poly E46 were generated from the original GLB, preserving the painted livery and wheels. These are sprite frames, not runtime 3D geometry.
- The car spins through several donuts on a black perspective grid with curved skid tracks and soft smoke that grows across the frame.
- The smoke reveals a bold, outlined `BRICK ECU` title before returning to the normal gauge dashboard.
- Lower-screen workspace buttons are immediately initialized; the previous lower-screen boot/status overlay is removed.
- Any key skips the animation without activating hidden menus or altering calibration.
- ECU fault, power-cycle, and safety prompts take priority over the intro.

## Build
The generated PNGs are **included**. Users do not need Blender, OpenCV, NumPy, or SciPy for the app intro build; these are optional regeneration dependencies for `art/render_intro.py` and `art/render_intro_fx.py`.
`make` invokes tex3ds to pack `art/intro_car.t3x` and `art/intro_fx.t3x`, and links the new embedded sprite assets.
The existing HOME Menu CGFX banner build remains unchanged from v5.0 and may have separate Python/Git requirements.

## Verification
- 29/29 Python regression scripts passed in two batches, after the source and assets were finalized.
- Native ECU write protocol mock: all six scenarios passed earlier in this revision.
- Host GCC C syntax passed with stubs (only pre-existing stub-related `linearAlloc` pointer-size warning).
- Native devkitARM compilation, tex3ds atlas packing, GPU runtime performance, and actual 3DS appearance **have not been verified** here.

The tuning screens, transport, calibration write path, and HOME Menu car-and-cable banner remain unchanged.
