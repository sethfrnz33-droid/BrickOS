# Current state versus target architecture

## Verified from the provided BrickOS v4.12 project

- Native USB-based ECU communications and firmware-specific INI lookup already exist in the underlying ESP project.
- The v4.9 VE learner detects a primary 16×16 U16 rusEFI VE table with load-major indexing, reads it, and builds **read-only** proposed values; its admission checks and heartbeat are implemented. It does not write those suggestions to ECU RAM or burn flash.
- The fuel-trim reader exposes available live per-bank values from matched INI definitions, with `Gego` interpreted separately from per-bank STFT/LTFT; LTFT table-page reading is not confirmed on live ECU hardware.
- The v4.11 guide reads an **explicitly authorized** session's samples, buckets idle/part-load/higher-load observations, tracks basic stability, lambda validity and faults, and returns seven advisory stages. Bank-2 trims are not included in its guide accumulation.
- The v4.12 motion receiver accepts optional session-bound IMU packets, handles sample-order/freshness checks, compares measurements against INI-resolved ECU speed and does not estimate torque or recommend spark changes.
- Host-side checks are documented in the firmware; connected ECU/ESP-IDF builds, timing synchronization and genuine motion calibration are **not** thereby proven.

## Not yet implemented

Full INI inventory; semantic rule registry; complete page/layout compatibility checker; functional dependency graph; persistent evidence database; start-event diary; open/closed-loop idle reasoning; per-parameter review coverage; generic expertly constrained calibration proposals; robust adaptive session coordinator; per-channel confidence model; synchronized external dyno data; 3DS Guided Calibration interface.

## Provenance discipline

Do not mark a capability `IMPLEMENTED` based on a document proposal, test fixture or endpoint name. Require `HOST_TESTED`, `HARDWARE_TESTED`, `ECU_VERSION_VERIFIED` and `PROFESSIONALLY_VALIDATED` as distinct labels. These are not substitutes for each other.

The design does not change the old manual editor or existing deployed firmware in this repository.
