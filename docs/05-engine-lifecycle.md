# Complete engine-lifecycle guided workflow

This is a **dynamic map**, not a mandatory sequential wizard. Hardware verification and professional validation are explicitly human-supervised. The ESP cannot certify an engine as safe for operation.

| Stage | Inputs and questions | Outcome | Hard blocks |
| --- | --- | --- | --- |
| Identity | Firmware signature, INI hash/pages, engine config revision | Compatible definition | Unknown layout / mismatch |
| Electrical/actuator readiness | Relevant pin/driver assignments, fault status, sensor plausibility | Requirements review | Unverified hardware or critical fault |
| Trigger/synchronization | ECU sync reports, trigger errors, physical reference validation record | Reference verified by qualified operator | Missing reference |
| First-start diagnosis | Event log, ECU states, faults, permitted sensor observations | Diagnosis with owner/next step | Unresolved critical checks |
| Cranking/catch | Separate cranking strategy; run transition | Temperature-bucketed evidence | No reliable sync/config |
| After-start | Temporal fuel/air/spark behavior | Pattern report | Sensor readiness unknown |
| Warmup | Temperature-based effects, idle and fuel changes | Coverage by thermal region | Bad coolant scale |
| Idle airflow | Actuator command, target/actual RPM, load response | Air-control diagnostic confidence | Invalid actuator strategy |
| Idle fuel | Active idle VE or main VE strategy, lambda/trim health | Idle fuel evidence | Stale/masked lambda |
| Idle spark | Existing spark corrections and stability | Analysis only | No independent support for timing edit |
| Closed-loop idle | PID state, saturation and oscillation evidence | Control-quality report | Unstable underlying model |
| Steady base fuel | Active fuel model, lambda target, trims, interpolated VE | Proposed offline fuel corrections | Unreliable measurements or model |
| Transient fueling | State transitions, delayed lambda, enrichment state | Event-quality classifications | Base fueling unresolved |
| Decel/fuel-cut recovery | Fuel-cut and return-state evidence | Recovery diagnostic | Transient/closed-loop ambiguity |
| Ignition characterization | Spark/knock/temperature/intervention context | Review and evidence gaps | No torque-quality proof |
| Optional strategies | VVT, ETB, boost, flex-fuel, auxiliary systems when installed | Dedicated dependent tasks | Strategy-specific invalidity |
| Instrumented validation | Professionally measured torque/power, synchronized ECU log | Bounded, comparable result | No trusted instrumentation |
| Final audit | Coverage, faults, changes, unresolved unknowns, saves | Report and explicit revision | Outstanding blockers |

## Completion and branching

Stages may be `NOT_APPLICABLE`, `NOT_STARTED`, `OBSERVING`, `NEEDS_REVIEW`, `BLOCKED`, `SUPPORTED`, `VERIFIED`, `EXTERNAL_VALIDATION_REQUIRED`. The user may open Manual Tuning at any time. Completion never implies overall tune safety.

If sensor calibration changes, only the graph-dependent evidence becomes stale. If a feature is disabled, tasks for it can be documented `NOT_APPLICABLE`, not automatically "complete". Optional features appear only if the discovered ECU configuration supports and enables them.

## First starts and dyno tests

No automatic first-start or high-load test execution is implied. The application records diagnostic observations and explains checks that require qualified human judgement and professional equipment. It must not suggest real-world vehicle tests or treat road motion estimates as a substitute for dyno validation.
