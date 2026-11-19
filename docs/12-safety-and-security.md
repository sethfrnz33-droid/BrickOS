# Safety, security, ownership and persistence

## Non-negotiable architecture

- rusEFI retains all hard-real-time engine control. The ESP must not become an uncontrolled replacement for engine safety systems, idle closed loop or spark scheduling.
- The provided v4.12 learning/motion modules remain read-only. Design documents do not authorize deployment of autonomous calibration writes.
- Manual Tuning has independent authority and remains operational. Guided proposals may not silently overwrite manual changes or an external tuning device.
- ECU identity, INI layout, page sizes and calibration revision are validated before use; unknown conditions block dependent actions.
- Device disconnect, heartbeat loss, invalid ECU transport, revoked authority, overrun or mismatch fail closed.
- Persistent flash burns are distinct from temporary RAM edits. A "Save" user-control must not be inferred from a UI page close or a reconnect.
- Unknown/poor wideband status, knock-related alerts or critical ECU faults block any dependent calibration proposal rather than encouraging further operation.

## Identity and transport threat model

Threats include anyone near an open Wi-Fi AP, session hijack, spoofed client IP, replayed commands, stale device after reconnect, simultaneous editor connections, corrupted INI/page data and a compromised bridge. The current v4.12 client-IP + 32-bit token design is not a sufficient long-term authentication system for live writes. Use paired-device authentication with modern authenticated integrity, replay protection and short-lived scoped leases before implementing any write authority. Never use the motion data receiver as a proxy authorization channel.

## Reviewable changes only

A proposal carries: source identity, original value, target field IDs, dependency hashes, scope, analysis evidence, conflict list, risk classification, approval identity, and review timestamp. Historical evidence alone cannot grant edit permission. Proposed changes have no effect until applied by a separately supported verified controller.

## Rollback semantics

Record the full original bytes, but do **not** promise that a blind mid-operation write of old values is always safe. Rollback is a controlled transaction requiring the same validation and authorization as the original write; if operating conditions are unsafe or uncertain, suspend rather than automatically mutating ECU RAM.

## Storage integrity

Persist evidence and configuration snapshots with checksums and version labels. Treat storage failure, low space, corrupt event log and clock discontinuities as separate problems. Make report export read-only by default. Never expose private tokens, credentials or vehicle identifiers in public logs.

## Review requirements

Require independent hardware verification and qualified supervision before any first-start, ignition or dyno calibration action. No unsupervised road-test recommendations; no algorithmic assurance of engine safety based on confidence scores.
