# BrickOS Guided Tuning — Executable Reference Prototype

**Architecture demonstration only. No ECU connection. No writes to RAM, flash, ignition or actuators.**

This is intentionally a self-contained Python standard-library implementation so the decision rules, INI inventory, API shape, control lease and deterministic tests can be exercised on any development machine. It is **not production ESP-IDF firmware**, and a TunerStudio/rusEFI INI's complete semantics are **not** implemented.

## Run

From this directory:

```bash
python -m brickos_guide demo
python -m unittest discover -s tests -v
python -m brickos_guide inspect ../path/to/your_generated.ini
python -m brickos_guide server
```

The optional local HTTP simulation runs only on `127.0.0.1:8765`, not on the ESP Wi-Fi AP. It has no ECU I/O. Its routes are clearly prefixed `/api/guided/v1/demo/` for mutations, while reports are read-only.

## Capabilities demonstrated

* Preserve unknown INI lines and unevaluated preprocessing directives so incomplete support is visible.
* Track section/page-qualified field keys, definition fingerprints, reported ECU signature and calibration revision.
* Invalidate downstream evidence after a simulated manual change.
* Require an unpredictable controller token and expiring heartbeat before new simulated observations.
* Assign explicit freshness/validity to readings and explain missing evidence.
* Flag idle instability, lambda validity, trim-masked fueling, trigger sync gaps, reported knock, and IMU limits.
* Return priority-ranked findings, audit events and paginated inventory through stable JSON contracts.

## Explicit limitations / next implementation steps

1. Replace the subset parser with a tested, full rusEFI/TunerStudio grammar including includes, expression evaluation, conditional compile and array/table variants. The sample parser must never be used to write to ECU memory.
2. Add strict, versioned semantic rules for each known firmware family, with a complete inventory report for all unknown fields.
3. Implement a single-source-of-truth ECU telemetry sampler with timestamped and correctly scaled bank-specific readings.
4. Build validated running-state classifiers; create independently tested experts for crank, afterstart, warmup, idle, fuel, decel, boost, transient and spark.
5. Implement persistent local evidence storage with integrity and versioning, a replay harness, and bounded RAM consumption.
6. Only after exhaustive simulation and qualified bench validation, consider scoped, explicitly authorized write proposals with separate readback and flash commit controls.
7. Add the optional 3DS UI after the ESP schema and stability tests have been validated.

Never infer best-torque ignition from MAP/airflow/acceleration alone. Do not claim a fixed sample window means the engine is calibrated.

## Full workflow planner

The `stages.py` model enumerates thirteen milestones from definition verification through supervised instrumented validation and final review; `registry.py` demonstrates a deliberately incomplete field semantic registry. Missing evidence remains visible and a stage never self-verifies just because enough samples arrived.
