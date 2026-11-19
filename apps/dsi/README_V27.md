# Brick Tuner Suite DSi v27 — nonblocking protected burn fix

This release fixes the v26 **interface freeze when pressing A to confirm a calibration burn**.
No 3DS application or ESP32 firmware files are modified.

## Confirmed root cause

In v26, the UI confirmation immediately called `bt_commit_field()` on the ARM9 main/UI thread. That legacy path performs a long series of **synchronous HTTP calls**: status, ECU original-data comparison, fresh stopped-RPM checks, ESP32 backup, ECU arm, changed-byte writes, multiple readbacks, flash burn, final readback, and disarm. A slow, dropped, or unacknowledged response held up both screens and button scanning. The Wi-Fi-loss callback could also cancel the pending socket without informing the burn routine.

## v27 changes

- START then A remains the confirmed trigger, but the burn now starts `bt_write_begin()` and advances with `bt_write_poll(frame)` through the existing nonblocking DSWiFi scheduler.
- Both screens continue rendering and processing VBlank while transactions are in progress. The lower screen shows the actual phase and progress.
- `B` can cancel **before** the ECU ARM request. Once ARM begins, cancellation is disabled and the screen explicitly says so, because a lost acknowledgment cannot safely be treated as a cancelled write.
- Each network request has a bounded idle wait (600 frames / approximately 10 seconds at 60 FPS) and an absolute 900-frame ceiling, even if a broken connection trickles bytes. There is no automatic retry of uncertain writes or burns.
- Wi-Fi disconnection notifies the transaction controller. If ARM has been attempted, a best-effort ECU disarm follows. Unknown write state is reported explicitly, not reported as success.
- Original safety gates remain: matching INI signature, ECU session/write/storage status, unchanged ECU baseline, fresh engine-stopped RPM checks, verified ESP32 backup, valid page permissions, verified byte writes, explicit flash burn, full readback, confirmed disarm.
- The UI retains local edits on refusal or uncertainty. The original data is replaced only after verified burn and readback.

## Host validation

Run from `DSi`:

```bash
python3 tests/run_v27_write_tests.py
python3 tests/run_write_tests.py
python3 tests/run_v26_tests.py /path/to/rusefi_proteus_f7.ini
```

v27's mock BrickOS/ESP server tests: successful verified burn, engine-running refusal, backup failure, ARM refusal, baseline mismatch, dropped status response, trickled/incomplete HTTP response, lost burn acknowledgment, explicit cancellation before ARM, refusal to cancel after ARM, and loss of Wi-Fi before/after ARM.
Also verified existing v22–v26 UI, parser, transport and protected-write host regressions.

**Hardware status:** Host tests pass; actual DSi + real ESP32/ECU write timing has not been verified in this environment. No new `.nds` has been built because devkitARM/libnds was unavailable.

If a v26 burn already appeared frozen, treat the ECU write as **uncertain** until its actual calibration is read back. Do not blindly repeat the burn or attempt this on a running engine.

## Build with devkitPro MSYS2

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```

Only replace the DSi application, not the 3DS project.
