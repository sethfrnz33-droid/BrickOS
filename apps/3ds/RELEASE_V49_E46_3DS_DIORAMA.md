# Brick Tuner Suite v4.9 — E46 + connected 3DS HOME Menu banner

- Enlarges the actual 3D E46 by 43% compared with v4.8, raises it in the HOME Menu frame and starts its 8-second rotation from a clearer three-quarter side view.
- Adds a tiny model of a dual-screen Nintendo 3DS with controls and a curved cable connected to the car, rotating as one scene so the cable remains attached.
- Adds a readable **BRICK ECU TUNER SUITE** title on a stationary camera-facing quad, not inside the tuner application's interface.
- Keeps the no-double-sided optimization and maintains a 512 KiB CGFX size guard.
- Adds texture-reduced and wheel-detail-reduced and smaller-scene fallbacks; the converter prints which source was accepted.
- Leaves `source/main.c`, ECU logic, calibration writes and gauge UI entirely unchanged from v4.8.

## Verification

Validated GLB structure, source geometry, animation endpoints and format; host regression tests and `bash -n` syntax checks. The CGFX output bytes and real hardware animation cannot be tested without the full pycgfx/dependency and 3DS setup. If the build falls back to an old E46-only GLB, the added console and title will not appear.
