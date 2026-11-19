# Brick Tuner Suite v3.5 — Source/host validation

*Source workspace validated: November 18, 2026 UTC. Hardware devkitARM build not available in the assistant container.*

Validation performed:

- v3.5 synthetic INI test compiled and executed actual category lookup functions; asserts ten functional groups, nine matching group types, maps leaf, and catch-all for unrecognized names.
- v3.5 source checks cover dual-screen RPM/CLT/AFR presentation, 0.001 direct precision, decimal entry, C-stick routing, non-modal fault status, SELECT+Y routing, persistent power-cycle prompt, curve focus navigation, write safeguards, and build manifest.
- Legacy tests from v2.1 through v3.4 passed after adapting the v3.4 tune-navigation expectation to the new explicit maps-return route. Included native tests are left unchanged.
- Host GCC syntax check succeeded; the stub implementation of `linearAlloc` produces a pointer-size warning on 64-bit host GCC, unrelated to the armv6k target. No known link failure from the duplicated `dreamfont` symbols remains in the source manifest.

**Not validated:** devkitARM compilation and link, console rendering, software keyboard on 3DS, exact calibration data from the user's ECU, reliable power-cycle detection, and bench-burn/write verification. The prompt deliberately requires user acknowledgement rather than claiming the ECU has restarted.

Run all project host tests from the project directory with: `for f in tests/test_*.py; do python "$f" || exit; done`

Build using: `cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh`
