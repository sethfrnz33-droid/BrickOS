# BrickOS development roadmap — design-first milestones

**No milestone is marked completed merely because these planning files exist.** Prior releases have partial functionality; this roadmap applies to the new full-INI Guided Tuning system.

## M0 — Compatibility and inventory [PLANNED]

- Build a versioned collection of real ECU-matched INIs and non-sensitive configuration fixtures.
- Verify signature hash/layout matching, page boundaries, signedness, enum and bit fields.
- Make the complete inventory and unknown/hidden field audit.
- Preserve all existing Manual Tuning behavior.

**Gate:** all fixture fields accounted for, no out-of-bounds reads, unknowns are clearly disclosed.

## M1 — Persistent evidence and state [PLANNED]

- Typed observations, freshness/exclusion flags, independent sample windows and replay.
- Event diary for startup, warmup, idle, cut, transient, sync and protection events.
- Configuration revision tracking, diff and dependency invalidation.

**Gate:** deterministic replay and reliable fail-closed behavior for stale input, resets and storage faults.

## M2 — Semantic graph + expert registry [PLANNED]

- Verified semantic rules, dynamic active-strategy resolution and graph edges.
- Stable finding IDs, hypotheses, missing-evidence logic and explanation trace.
- Configuration Atlas and rule-coverage reporting.

**Gate:** multi-version fixtures and dependency/reasoning regression suite.

## M3 — First integrated guided workflow [PLANNED]

- Sensor and first-start advisory, start/warmup diary, idle diagnostics and fueling/trim evidence.
- Simple `Next / Systems / Evidence / Atlas / History` API.
- No active ECU calibration writes.

**Gate:** bench-replay demonstrations where incorrect or ambiguous inputs never produce false authoritative advice.

## M4 — Fueling and calibration proposals [RESEARCH / CONTROLLED DESIGN]

- Interpolation-aware and bank-aware VE evidence, trim masking, transients and calibration region history.
- Offline proposals with audit trail, conflict detection, ownership and change review.
- Authorization and transaction design for future RAM-only operations; flash-saving separated.

**Gate:** extensive simulator/hardware tests, calibrated sensors and a verified write whitelist before any actual writable release.

## M5 — 3DS Guided Tuning interface [PLANNED]

- Separate top-level mode; manual editor unchanged; microcopy with evidence trace.
- Bounded, paginated deltas and inspectable diagrams; session-loss semantics.

**Gate:** real 3DS UI tested with disconnect/reconnect and no impact on ECU communications.

## M6 — Smart Spark evidence and dyno import [RESEARCH]

- Spark/intervention attribution, IMU validity and comparison gating.
- Professional dyno data synchronization/quality metadata and measured repeatability.
- **No automatic ignition changes** based on MAP, airflow or IMU alone.

**Gate:** qualified, independent evaluation with trusted instruments. The result may remain read-only permanently.

## M7 — Final evidence report [PLANNED]

- Complete INI coverage, validated subsystems, outstanding blocked/unknown tasks, configuration history and explicit save audit.
- Versioned export formats; transparent interpretation limits.

**Gate:** auditor can trace every conclusion to raw observations, ruleset version and exact ECU revision.

## Work-breakdown issue candidates

`ini/lexer`, `ini/semantic-registry`, `definition/fingerprint`, `coverage/audit`, `graph/invalidation`, `events/replay`, `sample/quality`, `expert/start`, `expert/idle`, `expert/fuel`, `expert/spark`, `api/v1`, `security/paired-controller`, `ui/3ds-guided`, `compatibility/fixtures`, `testing/bench`.
