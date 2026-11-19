# Starting, warmup and idle: detailed diagnostic logic

## Startup event segmentation

Each identified start retains separate `CRANK`, `CATCH`, `AFTER_START`, `WARMUP` and `IDLE_STABILIZED` periods, with confidence and time attribution. Store coolant/air temperatures, electrical supply, relevant ECU flags, synchronization, fuel/air/ignition commands, sensor readiness and faults. Event recognition is diagnostic; it must not autonomously command an engine start.

Associate events with the *applicable* cranking and after-start strategies, including overridden tables. Distinguish failure to synchronize from failure to run despite reported sync. Never infer injector or ignition hardware correctness solely from a sampled ECU output channel.

## Idle causal model

Idle stabilization depends on base airflow authority, control-loop authority, engine load, combustion consistency and spark/lambda corrections. An expert analyzes whether the ECU is genuinely in an idle-control state before attributing any RPM variation to idle PID.

| Observation pattern | Competing hypotheses | Missing evidence |
| --- | --- | --- |
| Target RPM misses while actuator appears saturated | actuator authority, hardware, load, airflow model | validated actuator/status and load state |
| Command oscillation appears before measured RPM change | feedback tuning, sensor/transport lag | synchronized command/RPM history |
| RPM changes before actuator command responds | external disturbance or combustion variation | load, lambda, ignition and fault context |
| Fuel trims non-neutral despite on-target lambda | closed-loop compensation masking base fueling | bank-scale validity and closed-loop state |
| Warm idle stable but earlier warmup unstable | temperature-dependent air/fuel/ignition or sensor issues | repeated events by temperature |

Every conclusion has a `not_proven` explanation and a direct link to relevant manual settings.

## Stage split

1. Verify idle type: mechanical stop, IAC PWM, stepper, DBW, other/unknown.
2. Verify target RPM curve and idle-entry conditions are decoded correctly.
3. Check plausibility of actuators, live feedback and their configured authority.
4. Characterize idle fuel with the actually enabled table.
5. Characterize spark corrections, without autonomous timing advance.
6. Evaluate closed-loop state, command response and saturation.
7. Assess repeatability under observed *normal* load and thermal variations.
8. Mark unresolved causes and professional verification tasks explicitly.

## Smart UI behavior

An "Idle" tile should show `Fuel`, `Air`, `Spark`, `Feedback`, `Temperature` and `Hardware` evidence—not a mysterious one-number score. Tap an explanation to open the *unchanged* original manual editor for that field. Manual editing updates the configuration revision and triggers dependency invalidation automatically.
