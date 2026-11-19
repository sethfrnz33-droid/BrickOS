# Architecture decisions

## ADR-001 — Guided Tuning is strictly opt-in

**Decision:** Keep Manual Tuning default and behaviorally unchanged. Guided Tuning may deep-link to it but may not rewrite UI organization, alter its permissions or silently change settings.

## ADR-002 — All interpretation is firmware/INI specific

**Decision:** Use exact ECU identity/layout plus active configuration. Infer no write permission from field-name resemblance. Filename prefix differences cannot cause a false mismatch when the validated firmware signature and layout are identical.

## ADR-003 — Account for every INI field; optimize only verified ones

**Decision:** Full inventory and coverage required; optimization needs a separately tested semantic expert and safety policy.

## ADR-004 — Evidence is not safety

**Decision:** Separate validity, precision, repeatability, calibration confidence and external verification. No generic "tune quality percentage" that implies engine safety.

## ADR-005 — Heartbeat is required, never sufficient

**Decision:** Session owner, authenticated scope, ECU match, revision and allowed operation must all be checked independently. The current IP/token scheme remains read-only.

## ADR-006 — RAM update is not flash burn

**Decision:** Any future write proposal is a separate transaction with readback; nonvolatile persistence requires a new explicit action under ECU-verified safe conditions.

## ADR-007 — Ignition remains evidence-first

**Decision:** No IMU/MAP/airflow-based automatic spark advance or claim of MBT. Professional torque-quality reference required for stronger conclusions.

## ADR-008 — Prior findings remain traceable

**Decision:** Configuration changes invalidate dependent evidence but never erase immutable history.

## Open design choices

- Persistence backend and storage budget on ESP hardware.
- Binary/JSON choice for paginated evidence transport to 3DS.
- How INI semantic rules are distributed/upgraded and signed.
- Which firmware families constitute the first validated compatibility set.
- Recovery behavior when the ECU reports a signature change mid-observation.
- Who can authorize a future write-capable tuning session and how paired controllers are managed.
