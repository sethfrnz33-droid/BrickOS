# Observation engine: sample timing, classification and confidence

## Data path

`ECU USB reads -> decoder -> timestamps -> freshness checks -> operating-state classifier -> per-expert evidence -> event log -> 3DS/web status summaries`.

The ESP's acquisition rate is limited by measured end-to-end USB reliability and ECU responsiveness, not an arbitrary throughput target. Use priority scheduling: safety/authorization events first; compatibility and user commands next; telemetry according to budget; bulk reporting lowest. Batch compatible output reads where supported. Track stale reads, retries, queue time, CRC failures, ECU task latency and reconnects separately.

## Sample provenance

Each sample stores definition hash, calibration revision, monotonic ESP time, ECU sample time when available, source, decoding status, freshness, relevant active corrections, session identity and acquisition timing. Never equate arrival time with original measurement time. For 3DS IMU samples, receipt timestamps alone do not establish synchronization.

## State classification (mutually exclusive primary states, optional overlays)

- `OFF`, `CRANKING`, `CATCH`, `AFTER_START`, `WARMUP`, `IDLE`, `STEADY`, `TRANSIENT`, `OVERRUN`, `FAULT/UNKNOWN`.
- Overlays: closed-loop fuel active, DFCO active, acceleration enrichment active, knock intervention, protection active, ETB limiting, sensor invalid, battery/thermal uncertainty.
- State transitions require guarded hysteresis and minimum observation windows; avoid chattering between idle and part-load.

## Sample eligibility is expert-specific

A sample valid for basic coolant-sensor diagnosis may be unsuitable for VE estimation. An event valid for startup analysis should not be admitted to steady-state fueling statistics. Report explicit exclusion reason codes rather than just a count of rejected samples.

Example reason codes: `MISSING_CHANNEL`, `STALE`, `BAD_SCALE`, `SENSOR_UNHEALTHY`, `CALIBRATION_CHANGED`, `RUNTIME_FLAG_UNKNOWN`, `TRANSIENT`, `FUEL_CUT`, `TRIM_MASKING`, `AFR_DELAY_UNRESOLVED`, `NOT_IN_ACTIVE_TABLE`, `CONTROL_INTERVENTION`, `TIME_ALIGNMENT_UNKNOWN`, `EXTERNAL_DATA_UNVERIFIED`.

## Evidence and confidence are separate

Store coverage, stability, source validity, consistency, sample independence and model applicability *individually*. Do not output a fabricated "87% safe" number. A display confidence label is a descriptive data-quality classification, not a probability of engine safety.

Reject correlated measurements as independent repeats. Downweight repeated samples from one unchanged condition when they add no information. Preserve outliers with reasons and maintain replayable raw observations where storage allows.

## Session reporting

Expose lightweight deltas and bounded event queues to the 3DS. Never block ECU sampling on rendering or Wi-Fi; backpressure drops UI updates rather than pushing sample latency into the acquisition task. Persist analysis only with clear storage budgets and explicit opt-in for history retention.
