# Engineering verification and acceptance criteria

## Test pyramid

1. **Parsing fixtures:** test multiple verified INI versions with scalar, bit, enum, array, curve, tables, missing channels, conditionals and alternative strategies.
2. **Fuzz and hostile INIs:** malformed scaling, overflows, invalid dimensions, page-boundary violations, duplicate labels, unknown tokens and inconsistent signatures. Failure must not produce a partially trusted active definition.
3. **Synthetic state replay:** known sequences of cranking/catch/idle, fuel cut, transients, trim masking, invalid lambda, stale transport, faults, manual edits and reconnects.
4. **Dependency tests:** changing a configuration subtree invalidates only the required related evidence; cycles/contradictions are surfaced, not silently resolved.
5. **Reasoning checks:** explanations reference measured evidence, competing hypotheses and missing data; confidence never used as a safety claim.
6. **Session/auth tests:** token theft/replay, client disconnect, missing heartbeat, signature swap, simultaneous clients, stale data and explicit stop all fail closed.
7. **Host test and static checks:** deterministic C library tests, memory/stack analysis, boundary tests, protocol schema checks and regression tests against the existing manual interface.
8. **ESP-IDF integration:** real compile/link and bounded-memory/CPU analysis, USB scheduling and responsiveness measurements, flash/storage behavior under power loss.
9. **Bench ECU/simulator:** verify INI pages, decoding, live channels, safe absence of write commands and fault injection using an isolated setup.
10. **Professional hardware validation:** only after prior gates and with qualified operators and appropriate instruments.

## Acceptance gates (initial four milestones)

**A — Atlas:** 100% raw field inventory accounting on each selected fixture, no out-of-range reads, no false semantic claims. Never equals 100% tune completion.

**B — Evidence:** each observation has provenance and exclusion reasons; stale and out-of-order samples never become valid evidence; UI backpressure does not change ECU acquisition latency beyond measured budget.

**C — Advisor:** at least one reproducible valid scenario and at least one ambiguous/failing case per expert. Every finding has an actionable explanation and stable identity. Unknown conditions return `HOLD` or `INSUFFICIENT_EVIDENCE`.

**D — 3DS separation:** enter/exit Guided Tuning without changing normal manual workflow; never enable edits on reconnection; UI rate cannot block ECU communication.

## Release labelling

`DESIGNED` = spec exists; `HOST_TESTED` = unit tests pass; `ESP_BUILT` = firmware compiled; `BENCH_VALIDATED` = instrumented ECU setup passed; `VEHICLE_PROFESSIONALLY_VALIDATED` = relevant assessment with appropriate oversight. Documentation must use the strongest *actually demonstrated* label only.

## Never assert from these tests

Host test success cannot establish engine safety, ignition MBT, accurate ECU protocol timing in the field, legal roadworthiness or complete hardware interoperability.
