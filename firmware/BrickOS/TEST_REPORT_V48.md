# v4.8 test report

- Host C syntax checks: PASS (main.c, storage.c, telemetry.c, USB SD importer)
- Host test of generated matching INI RPM (offset 4) / VBatt (offset 36): PASS
- New compiled test, no INI cache, exact firmware signature: PASS
- New compiled test, unknown firmware signature rejected: PASS
- Existing stim RPM/Batt policy tests: PASS
- JavaScript core + INI signature tests: 12/12 PASS
- ECU hardware and ESP-IDF target compilation: NOT TESTED in this environment

This release only changes the ESP32 trigger-preflight profile selection and error reporting. The entire source project is included.
