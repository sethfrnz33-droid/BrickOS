# Smart Spark, motion evidence and professional dyno integration

## The evidence boundary

Spark angle, knock activity, MAP, airflow, lambda, fuel use, VSS and an accelerometer can describe and compare operating conditions. They do not independently establish minimum spark for best torque (MBT) or a safe amount of advance. The absence of knock is not an advance permission. Engine geometry and fuel characteristics provide engineering context, not a verified numerical timing table.

## Three evidence tiers

**Tier 1 — ECU state:** actual spark, base strategy when resolved, corrections, faults, knock reporting, RPM/load, temperatures, lambda and fuel state. Supports diagnostics and correction attribution; cannot measure torque.

**Tier 2 — vehicle motion corroboration:** time-stamped ECU speed plus a calibrated rigid-mount IMU, gear/grade/motion-quality metadata where available. Supports data-quality and comparative motion consistency, *not* direct crankshaft torque or MBT. A handheld 3DS report of "aligned" does not prove mounting or calibration.

**Tier 3 — independent professional instrumentation:** validated torque/power measurements and synchronized ECU state with repeatability checks. Can support a bounded comparison under comparable conditions, with human approval and expert supervision.

## Ignition expert finding classes

- `IGNITION_REFERENCE_NOT_VERIFIED`
- `SPARK_COMMAND_VS_REPORTED_STATE_UNRESOLVED`
- `ACTIVE_CORRECTION_UNEXPLAINED`
- `KNOCK_OR_SYNC_EVENT_RECORDED`
- `MEASUREMENT_NOT_COMPARABLE`
- `IMU_ONLY_NO_TORQUE_PROOF`
- `TORQUE_DATA_REQUIRED`
- `NO_JUSTIFIED_SPARK_CHANGE`

## Geometry/fuel profile

Store bore, stroke, nominal compression ratio, fuel composition metadata, cam configuration and any verified combustion/knock-sensor information, with source/uncertainty. These are **not** sufficient to generate a reliable optimized ignition map. The UI may explain likely qualitative sensitivities but should not output authoritative degrees of spark advance from a simple formula.

## Dyno log logic

Import professionally recorded external instrument data with clock-alignment metadata, quality flags, instrument calibration and a configuration fingerprint. Validate comparable conditions and retain all confounders. Require independent repetitions before declaring a trend. Any timing proposal remains review-only unless engine-specific safety limits, rollback behavior, validation and operator authorization have been established.

## Explicit exclusions

No automatic spark advance, no IMU-driven spark command, no unsupervised load-seeking tests and no road-test prompts. Use a separate, controlled professional validation workflow for any active timing experiments.
