# Decision-rule worked examples (design, not firmware)

These examples define deterministic **advisory** behavior. They are intentionally limited to classification and evidence requests, not engine-control instructions.

## General admission contract

```
observe(sample, expert, configuration):
    require matched_ECU_definition_and_revision
    require each mandatory channel valid, fresh and correctly scaled
    require runtime state permitted by expert
    classify exclusions and store reason codes
    if accepted: update independently weighted evidence
    recompute hypotheses and missing_evidence
    emit versioned finding with the original field references
```

A sample can be *valid for one expert* but excluded by another. Example: a cranking event is valuable for startup diagnosis but inappropriate for warmed-up steady-state VE learning. No universal `good_sample` boolean.

## Decision Rule A: apparent idle control saturation

```
IF active idle strategy is verified
AND ECU reports stable idle mode and valid target/actual RPM
AND actuator status/command indicates limited authority for a sustained window
AND requested speed and measured speed disagree
THEN report AIR_CONTROL_AUTHORITY_QUESTION
WITH hypotheses [limited actuator range, hardware issue, external load, combustion variation]
AND missing evidence [independent actuator validation, load context, ignition/fuel context]
ACTION = REVIEW/DIAGNOSE, NOT change PID automatically
```

Block this finding if the command channel's scale is unknown or if the ECU was not in true idle mode.

## Decision Rule B: lambda looks perfect but trims are active

```
IF trusted lambda and target lambda are near each other
AND active feedback correction is confirmed and non-neutral
THEN mark BASE_FUEL_NOT_INDEPENDENTLY_VERIFIED
AND preserve bank-specific trim observations
AND prohibit direct conversion into a VE correction unless the firmware model is validated
ACTION = EXPLAIN CORRECTION MASKING
```

Never average bank 1 and bank 2 first and use that composite as universal fuel-trim truth.

## Decision Rule C: changed injector characterization

```
WHEN a verified change occurs in an injector model field:
    record original and new revision
    traverse affected fuel graph edges
    mark dependent steady-state VE evidence STALE_EVIDENCE
    retain earlier raw data/history
    do not invalidate unrelated display-only nodes
    prohibit a stale proposal from being written
```

## Decision Rule D: unknown INI setting

```
IF parsed field has no verified semantic expert:
    inventory state = NO_VERIFIED_SEMANTICS
    expose raw name, source location, unit and original menu path
    offer original Manual Tuning link where safe/defined
    prohibit optimization claim and writes
```

This is a successful *coverage* outcome, not a failed user workflow.

## Decision Rule E: motion data for spark optimization

```
IF IMU + vehicle speed agree within a verified sensor-quality model:
    report QUALITY_OF_MOTION_CORROBORATION
    do NOT report improved torque or MBT
IF no calibrated independent torque reference:
    report TORQUE_REFERENCE_MISSING
    spark adjustment recommendation = HOLD
```

## Decision Rule F: dropped 3DS session

```
WHEN authenticated controlling-device lease expires OR ECU identity changes:
    revoke any active future edit authorization synchronously
    stop issuing new tuning commands
    mark sample/evidence stream as disconnected or stale
    retain locally audited history if integrity is verified
    keep manual UI and ECU's own real-time controls independent
```

Reconnection must never implicitly authorize edits, even if it comes from the same IP.

## Decision Rule G: task priority

```
Candidates = all unresolved findings compatible with active ECU configuration
Remove findings already invalidated as noise or no longer applicable
First: hard safety/identity/measurement blocks
Next: newly invalidated prerequisite and regressions
Next: actionable diagnostics and evidence gaps
Last: optional improvements
Stabilize task choice with consistent tie-breaking and hysteresis
```

A missing sensor may surface several affected modules; show the common upstream problem, not ten duplicated recommendations.
