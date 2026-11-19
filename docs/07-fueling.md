# Fueling logic: VE, lambda, STFT/LTFT and transient events

## Structural requirements

Select the **active** fuel algorithm and base table, not whichever table has "VE" in its name. Resolve load source, RPM/load axes, table dimensions, storage types and interpolation behavior from the matching firmware definition and verified calibration. Bank/cylinder variants remain separate. A change to an axis invalidates previous cell location attribution.

## Closed-loop awareness

The existing firmware separates per-bank STFT/LTFT from legacy `Gego`. Preserve each source's native semantics and scaling, nullability, bank identity, status and timing. A lambda value on target while feedback is applying correction is not proof the open-loop base model is accurate. A closed-loop observation can inform the magnitude and direction of compensation *only when applicability and scaling are verified*; otherwise flag `TRIM_MASKING` and do not convert it into a VE estimate.

## Statistical model (proposal-only design)

Represent a trusted operating observation as an interpolation-weighted influence over the active calibration surface; aggregate multiple independent observations using robust estimators. Store the raw/target lambda relation, timing alignment uncertainty, correction context, sample provenance and weight distribution. Per-cell coverage should reflect physical independent visits and distribution of load/RPM, not merely duplicated telemetry samples.

Constrain offline proposals by verified table boundaries, cell locks, neighboring smoothness, calibrated sensor uncertainty and a cap relative to the immutable starting snapshot. No endless accumulating corrections to a reference table that has not been updated. A disabled/unknown model produces a diagnostic finding, not a guessed correction.

## Transient separation

Detect changes in TPS, MAP, RPM, wall-wetting/enrichment and fuel-cut state. Build distinct event windows for onset, recovery and stable intervals, recording the time lag to lambda measurement. Avoid teaching VE from events whose fuel behavior is dominated by transient strategy. A transient expert only evaluates patterns once basic steady-state fuel evidence is credible.

## Starting and thermal compensation

Record startup and warmup events with temperature-bucketed state and lambda-sensor readiness. Do not treat pre-warmup wideband readings as trustworthy unless the hardware indicates readiness. A single startup cannot validate a whole cold-start curve.

## Output example

```
Region: idle fuel / warm
Evidence: stable sensor windows exist, closed-loop compensation observed
Conclusion: base fueling accuracy not isolated
Missing: independent observations with trim interpretation verified
Next action: review active fuel/trim strategy in original editor
Policy: read-only evidence; no auto-write
```

## Roadmap after validation

Future professional calibration workflows may support reviewable RAM-only proposals and explicit commit separate from burn. No proposal has permission to run unless matched ECU layout, authority, current source revision, readback and qualified supervision requirements are met.
