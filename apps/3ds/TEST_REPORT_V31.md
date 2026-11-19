# Brick Tuner Suite v3.1 — Test Report

## Tested in this source container

- [x] GCC 14 host C syntax check using included 3DS compatibility stubs (not a devkitARM build).
- [x] **Native C + AddressSanitizer/UBSan**: exact INI log-axis matching, no guessed axes, incremental valid-sample coverage, undo/redo Z and X axis snapshot restoration and redo invalidation, bookmarks saved to SD-like paths and reloaded.
- [x] Source routes: Y from log replay → Trace map picker → ECU map; X toggles coverage; Back maintains the previous state.
- [x] Source routes: B leaving modified field warns, second B discards, A retains local edits.
- [x] v3.0 upper/lower tuning UI and semantic icon regression checks.
- [x] v2.3 folder-first list, return-position history, 9 gauge and 6 graph source checks.
- [x] v2.6 ECU fault parsing and status error shortcuts regression checks.
- [x] v2.6 INI auto-sync and stale INI-trigger checks.
- [x] v2.5 trigger preflight and signed start/stop mock order checks.
- [x] Six native calibration-write regression scenarios: success, backup failure, nonzero RPM, field mismatch, write rejection, burn rejection.
- [x] v2.1 gauge-hold and trigger-screen regression checks (build number contract updated to v3.1).

## Not tested here

- [ ] `make && bash build-cia.sh` under devkitPro / actual 3DS, as the devkitARM toolchain is unavailable.
- [ ] Frame rate, input feel, display legibility or audio output on 3DS hardware.
- [ ] Real rusEFI SD data logger integration or Wi-Fi transfer throughput.
- [ ] Burn, hardware outputs or ECU trigger stimulation on a physical board.
- [ ] SD bookmark persistence across actual FAT power loss/sleep scenarios.

**All tested functionality is source or simulated; do not interpret this as on-hardware certification.**
