# Calibration proposal/review transaction — future design only

**This specification describes a possible future controlled interface. The existing v4.12 ESP VE learner is read-only.** No API here is implemented and no direct engine changes are authorized by this document.

## Four independent objects

- **Evidence set**: immutable source samples, time alignment, channel validity, active corrections and revision hashes.
- **Finding**: explanatory diagnosis or unresolved hypothesis linked to evidence.
- **Proposal**: optional change candidate referencing exactly one trusted configuration snapshot, field/page identity, intended scope, constraints and conflicts.
- **Transaction**: explicit request to apply a separately authorized, whitelist-approved proposal to RAM with post-write verification, distinct from persistent save.

A hypothesis does not imply a proposal. A proposal does not imply permission to apply it. Successfully writing RAM does not imply a flash burn or safe tune.

## Proposed lifecycle

`DRAFT -> EVIDENCE_REVIEWED -> APPROVAL_REQUIRED -> AUTHORIZED -> PENDING_APPLY -> APPLIED_UNVERIFIED -> VERIFIED / ABORTED`.

Each transition is forbidden without required evidence, identity, permission and source revision checks. No implicit transition from UI presentation or heartbeat. After a conflict, the only valid next state is reconciliation or discard.

## Preconditions for any future write-capable release

1. Complete exact INI/page fingerprint and original-byte snapshot.
2. Semantic expert validated for the particular ECU firmware and operation.
3. Current configuration revision equals the proposal's source revision.
4. No competing writer and no critical ECU/transport fault.
5. Cryptographically authenticated, short-lived, operation-scoped user/device authorization.
6. Applicable constraints and operating conditions independently verified.
7. Safe bounded write transaction with readback and error classification.
8. Immutable log describing what actually changed and whether readback matched.

These are necessary conditions, **not proof that an engine-setting change is safe**. Specialist oversight is still required for high-consequence calibration.

## Persistence, discard and rollback

A separate explicit save operation must verify ECU-supported burn conditions and use the firmware's real commit semantics. On loss of device or mid-write faults, halt new changes and report the exact state. Never promise automatic restoration of old values during an unsafe or ambiguous engine condition; rollback may require a supervised procedure. Keep original calibration and external backups available for review.

## Conflict examples

- The user edits a cell in Manual Tuning after Guided Tuning generates a proposal: proposal is stale, block application.
- A new firmware/INI signature appears: all prior writable metadata becomes incompatible until reverified.
- ECU reports a different actual table than the proposal's fingerprint: block and show the discrepancy.
- Wi-Fi client reconnects after losing heartbeat: read-only browsing resumes; active write permission does not.
- External TunerStudio changes the same value: show revision conflict, never silently overwrite.
