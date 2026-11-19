# v3.6 host test report

- Source baseline: previous v3.5 release source, packaged as one full folder.
- New test: `tests/test_v36_split_screen.py`.
- Result: **15/15 host-side tests passed**.
- Verified: screenshot category mapping and fallback; top-screen INI children preview; lower-screen context action wiring; fixed six home gauges; live Wi-Fi/receive-Hz reporting; PTMU battery API lifecycle; fixed source manifest; existing guarded burn path; retained map precision and power-cycle acknowledgement.
- Compilation: `gcc -std=gnu11 -Wno-implicit-function-declaration -fsyntax-only -Itests/host_stubs -Isource source/main.c` passed. The host stub for `PTMU_*` mirrors the known libctru API. A legacy pointer-size warning from the host-only linearAlloc stub remains; this is not a real devkitARM build warning.
- Not run: devkitARM toolchain build, 3DS physical console visual inspection, live ECU integration testing.
