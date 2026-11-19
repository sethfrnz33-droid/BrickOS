# Guided Tuning: engine lifecycle and decision tree (design, not operating instructions)

**Purpose:** guide the user through an ECU-matched configuration from pre-start verification through idle, thermal behavior, steady-state fueling and professionally supervised instrumented validation. **This is not a procedure to start, load, or tune an engine unsupervised.** Read-only diagnostics are the first deployment target; manual editor is always separate and available.

## Entry: never assume every field is tuneable
1. Match actual ECU identity, generated INI/signature, storage pages, values, output channels, menus, transformations and visibility conditions.
2. Inventory every INI field with stable section/page/offset identity and mark: `decoded`, `active`, `understood`, `validation-supported`, `analysis-supported`, `proposal-supported`, or `unknown`. **100% accounted for** does not mean 100% auto-adjusted.
3. Discover hardware configuration and runtime strategies: crank/cam, fuel model, injectors, idle actuator, electronic throttle where applicable, lambda, temperature, closed-loop STFT/LTFT, knock, VVT/boost/flex if configured.
4. Construct a dependency graph. Any mismatched or unknown upstream parameter blocks only **dependent** experts, not the entire manual editor.
5. Produce an initial diagnostic summary and recommended next step with provenance and reason for every blocker.

## Lifecycle nodes and completion evidence
Each stage has `NOT_APPLICABLE / NOT_STARTED / OBSERVING / NEEDS_REVIEW / BLOCKED / VERIFIED / STALE` plus `evidence_count`, `missing_inputs`, `dependencies`, `manual_links`, and `validated_on_config_revision`. A stage can be verified only against a specified engine configuration and actually observed conditions.

| Stage | ESP looks for | Guidance produced | Why it can remain blocked |
| --- | --- | --- | --- |
| Identity and backups | matched signature, page layout, readable calibration snapshot | baseline established and backup provenance | mismatched INI / unreadable bytes |
| Sensors and assignments | sensor validity, plausible ranges, signal age, hardware mappings | checklist for external hardware verification | missing/contradictory sensor evidence |
| Sync and first-start readiness | reported sync, faults, configured injection/ignition reference | which assumption needs inspection | timing reference unverified |
| Start event diary | reports of crank, catch, after-start and transition states | compares repeated events by initial temperatures | insufficient repeated events / lambda not ready |
| Warmup characterization | CLT, IAT, sensor readiness, after-start and WUE active flags | temperature coverage and source of deviations | thermal regions missing / sensor uncertain |
| Idle airflow | target RPM, actual RPM, actuator command, idle state, reserve/saturation | competing airflow/control/hardware explanations | actuator data unknown |
| Idle fuel and spark | separate idle VE/spark if enabled; correction sources and lambda quality | separate reviews for base fuel and ignition correction | wrong table or trims masking |
| Idle controller | idle-entry, feedback correction, command response chronology | control response evidence and possible causes | oscillation cannot be attributed |
| Steady-state base fuel | target lambda, lambda, active table, trims, transient status | evidence-weighted VE proposals only | invalid delay/closed-loop semantics |
| Closed-loop STFT/LTFT | bank state, correction scale and apply/gathering mode | whether base fuel is being masked | unsupported bank/channel scaling |
| Acceleration/deceleration | enrichment, DFCO, recovery events and aligned lambda | event classifications; no false VE learning | baseline fuel not credible |
| Ignition review | measured actual spark, corrections, temperature, knock and confidence | caution/hold/needs instrumented validation | no trustworthy torque evidence |
| Advanced strategies | enabled subsystems only | subsystem-specific review | no verified rule for that variant |
| Instrumented validation | synchronized, externally qualified torque/pressure/other test evidence | comparison report, not automatic timing advance | inconsistent or absent instrumentation |
| Final audit | remaining blockers, revision history, backups and manual saves | evidence-backed report | any critical unresolved item |

## Causal ownership rules
- A cranking synchronization problem is **not** a VE error; do not propose fuel-table changes to hide it.
- A lambda deviation immediately after a throttle or fuel-cut transition belongs to **transient** analysis until proven steady.
- Stable lambda during non-neutral STFT/LTFT is **feedback-controlled**, not necessarily accurate VE.
- Idle oscillation may involve air, spark, combustion, external loads, control timing or sensors. Rank competing explanations; never tell users a parameter is guilty solely from correlation.
- Separate idle VE/spark or after-cranking taper strategies must be used **only when active in ECU calibration**.
- If external settings or manual tuning change, invalidate downstream conclusions rather than replay old proposals.
- Fuel, air, and ignition learning engines cannot simultaneously edit dependent controls. Only one proposed calibration family can be considered active, and future approved edits are serialized.

## Guided question selection
Ask questions only when the answer materially changes a decision and cannot be read from the ECU or INI. Differentiate:
- **ECU-observed:** signature, configuration, flags, live outputs.
- **User-provided:** engine hardware properties, equipment capabilities, whether external physical verification occurred.
- **Calculated:** time-aligned error, interpolation weights, confidence and coverage.
- **Inferred:** likely cause based on evidence; show alternatives.
- **Unknown:** unavailable or unverified; do not substitute a default.

Every task should answer five things: `What I found`, `Why it matters`, `Other possibilities`, `Evidence needed`, `Open exact manual setting`. The guide may suggest a *diagnostic check*, but must never start the engine or control an actuator on its own.

## Prioritization
Priority is lexicographic, not a vague numeric magic score:
1. safety/identity/configuration faults,
2. data and sensor validity,
3. unmet prerequisite,
4. regression after a known change,
5. evidence gap in an active stage,
6. optional refinement.
Use sticky task IDs and hysteresis to avoid a changing interface every telemetry frame. If a task cannot be resolved from existing sensors, present an explicit manual verification item instead of repeating an unanswerable prompt.

## Special case: start-event and thermal history
Collect timestamped event boundaries and store pre-event temperature, relevant ECU state, lambda readiness, RPM behavior and possible faults. Events are compared only when configuration revision and environmental/engine conditions are sufficiently comparable. Missing cold-weather samples remain **unobserved**, not bad.

## Special case: idle diagnostic branches
- **Command reaches a limit / RPM misses target:** inspect control authority and possible hardware/load constraints; cannot conclude that PID gains are wrong.
- **Command oscillates before RPM:** investigate feedback/control relationship and signal delay.
- **RPM fluctuates without a matching command change:** investigate combustion, fueling, ignition and load.
- **Lambda target achieved while fuel corrections persist:** note trim masking.
- **Behavior varies with coolant temperature:** connect to WUE/ASE/idle target applicability, not automatically to main VE.

## Special case: VE learner and table coverage
Coverage is a map of **measured qualifying evidence**, not a map of engine safety. A 16x16 table is internally divided into operating regions; the user never needs to manually visit all 256 cells. Unsupported or unobserved regions show `not evaluated`. Cell locks, bank/strategy choice, target table and sensor-delay assumptions are explicitly visible in advanced review.

## Special case: ignition and virtual dyno
Combine engine configuration, actual spark, correction channels, MAP, airflow, knock observations and qualified motion measurements only as **context and diagnostic evidence**. A phone/3DS accelerometer is not a torque transducer; unknown road gradient, mounting and drivetrain effects limit conclusions. Show `needs instrumented confirmation`, not `MBT found` or a numerical degree suggestion derived from sensor correlation alone. Any performance validation is reserved for suitably supervised, controlled facilities.

## Exit and completion
`Guide reviewed` = every applicable INI item is accounted for with explicit status. `Configuration verified` = specified assumptions checked. `Fuel evidence adequate` = defined areas supported by qualifying independent observations. `Professionally validated` = external evidence explicitly provided. **These four states are independent**; there is no single misleading `100% tuned` percentage.

## Suggested built-in guidance capabilities
- Next best supported task with full explanation.
- Start diary, idle stability breakdown, warmup coverage and fuel-trim bank comparison.
- Heatmaps of accepted/rejected samples with reasons, source and timestamp.
- Calibration dependency impact preview before any manual change.
- Mode-aware search of INI settings and direct deep-link to existing manual dialog.
- Snapshot comparison, regression annotations, evidence replay and session audit.
- Accessible fault priority and clear unknown/unavailable source labels.

## Source references
rusEFI configuration guide (idle VE, ignition, cranking, STFT/LTFT): https://rusefi.com/docs/guide/
rusEFI generated TunerStudio template: https://github.com/rusefi/rusefi/blob/master/firmware/tunerstudio/tunerstudio.template.ini
TunerStudio warmup analysis UI and filtered-observation model: https://www.tunerstudio.com/index.php/products/tuner-studio/tsarticles/96-warmup-enrichment-wue-analyze
