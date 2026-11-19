# Guided Tuning: ESP autotune runtime and controls (design specification)

**Status: proposed architecture, not deployed firmware.** Current imported ESP source is v4.8, and `prototype/` is an offline read-only reference. This document does not authorize ignition optimization, high-load testing, unattended engine operation, or ECU writes.

## Core separation
- **ECU:** real-time engine control, failsafes, sensor conversion, persistent calibration.
- **ESP:** dedicated ECU transport, immutable evidence collection, active-definition resolution, expert analysis, proposal ledger, arbitration and session supervision. ECU traffic is scheduled separately from HTTP/Wi-Fi delivery.
- **3DS / phone:** request a guided session, set permissible analysis boundaries, review explanations and proposals, and explicitly issue actions. They do not calculate real-time fueling or keep the ECU stable.
- **Manual Tuning:** remains a distinct, independently navigable experience with unchanged INI tree, editor and burn path. A manual change invalidates conflicting guided proposals; the guide never overwrites it.

## Three permission modes
1. **PASSIVE MONITOR:** opt-in read-only observations, no controller lease needed, safe to browse history after reconnection.
2. **GUIDED LEARN:** authenticated controller lease + heartbeat, keeps active evidence windows and read-only proposals; stops processing on lease loss.
3. **CONTROLLED RAM APPLY (future, not enabled):** separate, short-lived, operation-scoped authorization, verified matching ECU and INI, exclusive writer, field whitelist, original calibration snapshot, specific review and readback. Specialist-supervised commissioning required. No background ignition advancement or autonomous flash burn.

Session state machine:
`DISCONNECTED -> DISCOVERING -> VERIFIED -> OBSERVING -> LEARNING -> REVIEWING -> [AUTHORIZED_RAM_APPLY] -> VERIFYING -> REVIEWING -> EXPLICIT_COMMIT`.
Any transport, identity, lease, sensor or safety failure transitions to `SUSPENDED` or `FAULT`, with a recorded reason; reconnection does **not** restore permissions. Stop, pause, discard proposals, restore a known backup and burn/save are **different commands** with different authorization.

## Timing and scheduling
The acquisition task is decoupled from the Wi-Fi publishing task. Sampling frequency is the **measured safe sustained ECU read capacity**, not a promised fixed rate. The scheduler prioritizes transport health, engine faults and mandatory acknowledgments before background logging, large INI transfers or device status requests. Every measurement receives ECU/source timestamp when available, ESP monotonic receive time, sequence number, configuration fingerprint, units and freshness status. USB writes may not be interleaved with unrelated reads unless the specific transport protocol permits this.

The UI uses compact summaries and a monotonically increasing revision cursor; a 3DS may request fewer updates than a phone. Dropped display frames never cause a backlog of control requests. The last known state is labeled stale rather than shown as live.

## VE learner: explicit processing sequence
1. Resolve the **active** fuel algorithm, correct bank, selected VE/trim table, axes, table dimensions, target lambda and output channels from the matched INI **and current ECU settings**. A `veAnalyzeMap` stanza is a useful definition, not proof a given feature is active. Unsupported strategies stay read-only.
2. Validate health: ECU identity, sync, temperatures, fuel pressure if instrumented, wideband readiness and validity, telemetry freshness, configuration revision, closed-loop/trim status and relevant ECU fault or protection flags.
3. Classify operating state: crank/after-start, thermal transition, steady idle, stable running, throttle change, acceleration enrichment, DFCO, restart recovery, fuel strategy change, or indeterminate. Never feed an ambiguous/transient event to steady-state VE.
4. Apply user-visible acquisition boundaries: minimum/maximum RPM, TPS and load bounds, coolant range, permitted region mask, dwell/stability limits, minimum independent visits, lambda delay handling, exclusions after state transitions, battery/sensor-health constraints, and sample age.
5. Normalize lambda/target only with verified units; separately preserve bank-specific STFT, LTFT, `Gego` and correction status. On-target lambda while fuel trims compensate is **not** evidence that base VE is correct. If correction semantics are unknown, mark trim masking and withhold the VE estimate.
6. Time-align the measured lambda response with the earlier fuel/air operating condition, using a verified delay model and its uncertainty. If delay uncertainty is too large, collect evidence but do not train cells.
7. Attribute eligible evidence to **interpolated neighboring cells** of the actual ECU lookup surface, not just the nearest center. Aggregate independent visits, reject outliers, reduce weight for repeated correlated samples, and retain per-cell provenance.
8. Produce a **proposed** smooth correction surface, with locked-cell exclusions, absolute and relative caps tied to an original snapshot, margin and smoothness checks, separate bank treatment, and uncertainty bounds. No provisional estimate may automatically reset another correction table.
9. Report exactly what was learned: cells observed, independent revisit count, confidence/uncertainty, active exclusions, lambda/trim context, highest-impact pending findings, and original-versus-proposed diff.
10. Stop proposing on changed axes, tune revision, ECU mismatch, stale lambda, communication loss, control lease expiry or critical ECU faults. Previously collected data remains labeled with its original revision.

**No circular learning:** a learner must not learn from its own changes until subsequent independent observations pass a new validation epoch. Calibration revisions are immutable anchors.

## Configurability contract
**Basic:** chosen region and strategy, observe/learn state, status, reason for rejected data, target/source confirmation, proposed change preview. 
**Advanced (not all visible by default):** min/max RPM, TPS, MAP/load, coolant and IAT, lambda window, cell dwell, sample stability, throttle/RPM/MAP derivative filters, AFR transport delay profile, exclusion time after transients/DFCO, allowable closed-loop states, bank selection, confidence threshold, required independent visits, cell locks, interpolation/smoothing preference, session change cap, source table, alert severity and logging policy.
Settings are **analysis boundaries**, not ways to disable ECU protection; incompatible selections fail validation and show a reason. Profiles are versioned against the matching INI and not blindly transferable across engines.

## Proposed device-facing API (not implemented)
- `GET /api/guided/v1/capabilities`: ECU signature, INI fingerprint, modules and reason each is supported or not.
- `GET /api/guided/v1/overview`: state, primary next task, confidence categories, active filters, alarms and revision cursor.
- `GET /api/guided/v1/stages`, `/fields`, `/evidence`, `/events`: paginated discovery and read-only evidence.
- `POST /api/guided/v1/session/start`, `/heartbeat`, `/pause`, `/stop`: authenticated state transitions; operation idempotency keys and expiry.
- `POST /api/guided/v1/analysis-config`: validate/store analysis boundaries; return effective settings and INI-dependent constraints.
- `GET /api/guided/v1/proposals`: read-only candidates and explanations.
There is **no public write/burn endpoint** in this planning milestone. Future write actions require a separate reviewed security/API specification, not a reused learning token.

## Security and fault policy
The imported ESP v4.8 Wi-Fi AP is open; network reachability **must not** grant actuator or calibration authority. A future write-capable system needs secure pairing, strong session authentication, freshness, replay prevention, and the ability to distinguish controller from spectator. A heartbeat is necessary but **not sufficient** authentication. Device loss suspends new operations; it must never trigger an unverified emergency calibration rollback. All aborts and incomplete transactions are exposed to the user, including the exact last acknowledged ECU revision.

## Source-grounded reasons
- rusEFI TunerStudio template defines `veAnalyzeMap`, `egoCorrectionForVeAnalyze`, and customizable RPM/CLT/TPS/battery filters: https://github.com/rusefi/rusefi/blob/master/firmware/tunerstudio/tunerstudio.template.ini
- rusEFI explicitly separates STFT/LTFT and allows standalone LTFT application: https://rusefi.com/docs/guide/
- TunerStudio WUE analysis visibly separates sampled, filtered and recommended data and differentiates RAM edits from persistent burn: https://www.tunerstudio.com/index.php/products/tuner-studio/tsarticles/96-warmup-enrichment-wue-analyze
