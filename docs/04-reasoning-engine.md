# Reasoning engine, hypothesis ranking and task prioritization

The advisor is **not** a collection of loosely coupled threshold alarms. It maintains causal hypotheses, observable predictions, uncertainty and verifiable exit criteria.

## Finding lifecycle

`DETECTED -> TRIAGED -> INVESTIGATING -> EVIDENCE_SUFFICIENT / AMBIGUOUS -> REVIEWED -> VALIDATED / REOPENED`. Blocking faults may force `SUSPENDED` for dependent subsystems. Findings survive app restarts as long as their source data and revision remain compatible.

## Expert output contract

```
Finding {
  id; expert_id; operating_region; severity; status;
  concise_problem; why_it_matters; relevant_field_ids[];
  evidence_refs[]; exclusions[]; competing_hypotheses[];
  confidence_breakdown; missing_evidence[];
  recommended_action; prohibited_actions[];
  required_verification; related_graph_nodes[];
  applies_to_revision; last_updated;
}
```

Every recommendation must answer: *What was observed? What could cause it? What is the best-supported explanation? What evidence would discriminate the alternatives? What stops this advice from being applied automatically?*

## Hypothesis example: unstable idle

Possible explanations include airflow actuator authority, mechanical load, feedback-controller oscillation, inconsistent combustion, fuel delivery, spark corrections and sensor faults. Analyze **phase/order of events**, not just averaged RPM error: did actuator command precede RPM variation or follow it? This is evidence but not proof. If only engine speed exists, return `INSUFFICIENT_DATA` instead of inventing a cause.

## Priority policy

Order by *hard blocks first*, followed by invalidated prerequisites, regressions, unresolved diagnostics, missing validation evidence, supported improvements and optional exploration. Stable tie-breakers: severity, downstream impact, age, user-selected goal, evidence needed, and the ability to resolve with read-only observations. Avoid flapping using debounce and sticky task selection. A low-confidence improvement never overrides a hardware-fault block.

## Recommendation output classes

- `VERIFY_CONFIGURATION`: compare actual config with known required references.
- `INSPECT_HARDWARE`: calibration cannot resolve the evidence.
- `COLLECT_PASSIVE_EVIDENCE`: observe during ordinary supported operation.
- `REVIEW_TABLE`: show directly linked manual calibration region.
- `REVIEW_PROPOSAL`: present a separately generated, bounded offline estimate.
- `REQUIRES_PROFESSIONAL_VALIDATION`: instrumented evaluation required.
- `HOLD`: existing evidence does not justify change.

## No self-confirming conclusions

An advisor that makes a suggestion may not mark it successful merely because the next reading resembles the expectation; require independent validation and account for environmental changes, active compensations and other simultaneous edits. For ignition, no numeric MBT claim from acceleration/MAP/knock absence alone.
