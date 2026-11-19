# BrickOS v4.2 — Built-in calibration writing (ESP32 firmware)

**Read [README_V42.md](README_V42.md) first** for build/flash and runtime verification.

BrickOS v4.2 always compiles the rusEFI C and B protocol into the ESP32 firmware; there is no optional `BRICK_ALLOW_CAL_WRITES` build flag. Writes are authorized temporarily by the editor after stopped-engine checks, full-page backup, and conflict detection. Actual flash persistence has not been verified on ECU hardware.
