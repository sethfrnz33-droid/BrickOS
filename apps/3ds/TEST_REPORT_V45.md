# v4.5 Test Summary

23/23 regression suites: PASS.

`tests/test_v45_ui_and_burn.py` compiles and runs the exact bounded write-planning helper with native GCC; tests zero edits, scattered edits, boundary crossing, unchanged gaps, and 128-byte bounds, plus checks rendering call sites and ECU safety checks.

`tests/native_mock_write.py` passes all six ECU protocol scenarios (success, backup rejection, RPM not stopped, stale baseline, chunk write rejection, flash burn rejection).

The host environment has no installed devkitARM and no physical 3DS/ECU, so build-time native 3DS compatibility and speed improvements have not been verified on hardware.
