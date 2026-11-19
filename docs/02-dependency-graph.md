# Dependency graph and calibration invalidation

## Types of relationships

- `REQUIRES`: Verification of A needed to trust B.
- `DERIVES_FROM`: B uses A numerically in its calculation.
- `CONSTRAINED_BY`: B requires A to remain inside a verified safety envelope.
- `OVERRIDDEN_BY`: Another enabled strategy can suppress B.
- `OBSERVED_BY`: Telemetry signal associated with a subsystem, not proof of cause.
- `CONFLICTS_WITH`: Two pending proposals cannot be applied together.
- `INVALIDATES_IF_CHANGED`: Changes in A invalidate evidence previously collected for B.
- `HAS_ALTERNATIVE`: A different active strategy changes the workflow.

All edges identify their rule ID, firmware applicability and reason. Detect cycles; distinguish meaningful feedback loops (physical control) from invalid prerequisites (workflow DAG).

## Example chain

Sensor scaling and injector characterization -> lambda/trim interpretation -> base fueling evidence -> transient-fuel observations -> idle-fueling confidence. Separate path: ignition reference -> actual-spark interpretation -> spark evidence. The graph is dynamic: it is filtered by the *active* ECU strategy, not all theoretically possible strategies.

## Revision and invalidation

Store a hash of **each relevant calibration subtree** (and a whole-config hash), not only an opaque global checksum. A manually edited injector value invalidates certain fuel learning but does not automatically erase unrelated display preferences. Changes in fuel model, load-axis breakpoints or live-channel scaling invalidate a wider part of the graph.

Three invalidation actions:

1. `HARD_BLOCK`: prerequisites or compatibility no longer trusted.
2. `STALE_EVIDENCE`: previous observations exist but cannot validate the new revision.
3. `REVIEW_REQUIRED`: change may be relevant; cannot prove it matters from the available data.

Old data remains immutable and queryable. Never delete audit history merely because evidence becomes stale. If a manual edit races a guided proposal, halt guided application, re-read the relevant ECU bytes and require an explicitly reviewed rebase; do not overwrite the manual edit.

## Completion contract

A node can be `VERIFIED` only if its required diagnostics, approved validation method and relevant version fingerprints still match. `VISITED`, `SAMPLED`, `CONFIGURED`, `PLAUSIBLE`, `VERIFIED` and `PROFESSIONALLY_VALIDATED` are different states.
