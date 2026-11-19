# ESP authority and proposed device protocol

## Existing baseline routes (observed in v4.12 documentation)

- `/api/autotune/status`, `/api/autotune/table`, `/api/autotune/config`
- `/api/autotune/prepare`, `/api/autotune/start`, `/api/autotune/heartbeat`, `/api/autotune/stop`
- `/api/tune/guide`, `/api/fueltrims/live`, `/api/fueltrims/tables`
- `/api/tune/motion`

The current learner is read-only. Existing endpoints are not evidence a new feature exists. Do not redefine them with breaking semantics.

## Proposed read-only `v1` namespace (NOT IMPLEMENTED)

| Route | Method | Purpose |
| --- | --- | --- |
| `/api/guided/v1/capabilities` | GET | ECU/INI fingerprint, supported experts and feature gate status |
| `/api/guided/v1/inventory` | GET | Paginated complete INI field inventory and reason codes |
| `/api/guided/v1/graph` | GET | Dependency graph subtree and invalidation policy |
| `/api/guided/v1/stages` | GET | Lifecycle stage states with blockers |
| `/api/guided/v1/findings` | GET | Prioritized findings, filter and cursor |
| `/api/guided/v1/findings/{id}` | GET | Evidence, hypotheses, missing data and manual-editor references |
| `/api/guided/v1/evidence` | GET | Bounded sample/event summaries, never unbounded raw responses |
| `/api/guided/v1/events` | GET | Session/event history with revisions and timestamps |
| `/api/guided/v1/manual-link/{field}` | GET | Canonical manual tree/dialog/table reference |
| `/api/guided/v1/report` | GET | Snapshot of assessed/unknown/unverified systems |

## Coordinator state machine

`OFF -> DISCOVERING -> VERIFIED_READ_ONLY -> OBSERVING -> REVIEWING` with edges to `SUSPENDED`, `DISCONNECTED`, `INCOMPATIBLE`, `FAULTED` and `OFF`. It may not transition into future `PENDING_WRITE` without a new, independently authenticated, scoped operator permission and a whitelist-verified capability. A heartbeat is necessary but insufficient for write authorization.

## API contract principles

Return `schema_version`, `definition_hash`, `configuration_revision`, `session_id`, `sample_time`, `freshness`, and explicit nullability. Distinguish `unsupported`, `disabled`, `missing`, `stale`, `invalid`, `not_collected`. One endpoint failure may not be interpreted as a zero-valued sensor.

Client display subscriptions never authorize learning, live writes or burns. Losing a device session instantly revokes active permissions and stops future automatic commands. Auth tokens must be cryptographically unpredictable and protected; binding by IP and a small numeric token alone is not adequate security on an open AP.

## Future write capability (nonimplemented)

Design a separate transactional service: snapshot, inspect exact target range, require owner/controller authorization, check session lease and version, apply only reviewed whitelisted RAM edits, read back, audit, and revoke on any mismatch. Flash persistence requires another explicit approval under ECU-verified permitted conditions. This is not part of the current backend.
