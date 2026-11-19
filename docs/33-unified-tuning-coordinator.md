# BrickOS Unified Guided Tuning Coordinator — Product Contract V1

> **Design specification, not implemented production firmware.** Current ESP v4.8-derived firmware exposes telemetry and browser tuning; existing phone Seth dialogue and reasoning are read-only browser-side. The proposed high-rate recorder, ESP-owned diagnostic agent, engine archive, calibration coordinator and automated RAM writer are future work. Manual Tuning, 3DS and DSi connection paths stay unchanged.
>
> **Unsupported:** GDI and mixed PFI/GDI calibration are expressly **OUT OF SCOPE** for BrickOS Guided Tuning. Broad rusEFI support for some GDI hardware does not grant Brick ECU hardware/software GDI capability. Detect, explain and block the unsupported path instead of offering imagined GDI procedures.

## One system, three distinct roles

**Seth = negotiator/teacher/case manager.** Maintains engine archive, knows current user goal, routes one question at a time, decides which expert owns the issue, explains what the ECU can and cannot verify, and records human acknowledgements. Seth does not invent the answer or execute a physical engine test.

**Expert engines = evidence and estimates.** INI resolver, sensor model, starting/sync analysis, idle expert, steady fuel/trim learner, transient expert, ignition reviewer, event recorder, and configuration diff/audit. Each advertises `understand / diagnose / learn / propose / controllable` capability independently. Supported roles and confidence differ per engine configuration.

**Coordinator = safety, workflow and decisions.** Computes the stage dependency graph from the ten conceptual HP Academy chapters plus the actual config, reconciles the profile against the ECU, arbitrates observations/sessions, and defines which actions are merely explained, manually verified, automatically analyzed, or eligible for future qualified, specifically authorized RAM-only adjustments. The browser and future DS only render portable, versioned packets.

No one subsystem may silently promote an explanation into an ECU command. **The current release has no permitted autonomous ECU calibration actions.**

## Three sources of truth (never collapse them)

1. **Engine build profile:** durable local archive describing physical hardware and facts provided/externally verified by the user; survives ECU definition changes. Claims have source, date, revision and verification status.
2. **Connected ECU snapshot:** actual signature, generated INI/layout, decoded configuration, live output channels, active table/feature flags and calibration revision. INI contents describe representations, not a guarantee the hardware matches.
3. **Measurement/case evidence:** immutable logs, event windows, health flags, actual active correction state, trace IDs, known timestamp uncertainty, compareable baseline context and instrumentation reports.

Merge only after comparing consistency. `user says pressure sensor exists` + `INI has fuelPressure` does not prove the sensor is wired, calibrated, healthy or reporting real rail pressure. Show `reported`, `INI-configured`, `ECU-observed`, and `externally verified` separately.

## Front-door user experience

Seth home contains: **Full guided tour**, **Help with a problem**, **Assess current condition**, **Analyze an event**, **Continue last project**, **Engine build archive**. From any mode, `Open original INI setting` returns to the existing untouched Manual Tune editor. The guide never blocks entering Manual Tune.

### Minimum discovery questionnaire — ask progressively

Ask only questions that affect the chosen step. Do **not** dump a universal survey before showing the user an engine.
- Identify this engine: optional nickname/project label, vehicle/engine family, known cylinders, displacement and other relevant verified hardware information.
- Fuel injection architecture: `port injection (PFI)` / `direct injection (GDI)` / `mixed` / `unknown`.
- Fuel: reported composition/fuel type, availability of verified fuel property or composition sensor, known injector type and characterization reference.
- Fuel delivery: pump command method **separate from** pressure regulation. Pump command = relay/on-off, PWM through verified compatible controller, ECU-independent OEM controller, other or unknown. Regulation = mechanical manifold-referenced, fixed/atmospheric, electronically regulated feedback, none/unknown, hybrid or other.
- Measurements: what independent fuel-pressure measurement exists? Is a rail pressure sensor present, interpreted as absolute/gauge/differential, and independently validated? What do the ECU and external tool actually report?
- Air/ignition/sync/sensors: requested only at relevant chapters and matched to actual INI capabilities; physical details from the user may remain unverified.
- Constraints: instrumentation, hardware verification, available data logs, qualified professional checks, and whether the person is reviewing a past session or only observing.

For every question support `Not sure`, `Explain why`, `Use ECU-detected answer where reliable`, and `Save for later`. Unknown must never be auto-converted to a guessed conventional engine value. The profile remembers answers independently from single-phone session state.

### Engine profile dialogue example (original Seth, not HPA content)

> "Okay, whose engine am I trapped in today? Tell me what it's actually built with, not what the base tune happens to claim."
>
> "You said the pump uses PWM. Fair. That tells me how it's commanded, **not** whether it has a regulator or how rail pressure is measured. Does another controller actually regulate pressure, or is that still unknown?"
>
> "If pressure feedback is present, I can compare what the ECU reports with the configured reference. I still need that sensor's calibration and physical result verified; I'm a clone in a computer, not a pressure gauge."
>
> "This configuration says direct injection. This BrickOS Guided Tuning release doesn't support GDI, so I won't generate a fuel strategy, injector plan or autotune recommendation for it. You can still view the general ECU information without pretending the fuel system is covered."

Seth's dry humor is occasional, but blocking problems and unknown sensor data are stated plainly.

## HP Academy-inspired workflow becomes a capability-aware stage planner

The published ten stages remain **navigation and learning order**, not automatic physical test instructions. Every step generates a `TaskPlan` with responsible expert, required engine-build facts, required ECU fields, currently matched evidence, source revision, and **execution class**:

- `EXPLAIN`: Seth teaches what the setting/measurement means with source citations and caveats.
- `USER_VERIFY`: qualified user/technician performs a nonautomated physical/configuration verification; user-supplied result stays labeled reported unless independently corroborated.
- `PASSIVE_ANALYZE`: existing telemetry or optional controlled-session event recording can assess signal patterns without controlling the engine.
- `LEARN_PROPOSAL`: a verified analysis module may generate a **review-only** estimate with uncertainty and original-versus-suggested values.
- `AUTHORIZED_RAM_APPLY`: **future, disabled**, only after ECU-version validation, security/pairing, exclusive writer, source snapshot, bounded operation, hardware supervision and readback. Never a default action.
- `UNSUPPORTED`: no claim of capability; don't expose an actionable flow.
- `EXTERNAL_VALIDATION`: professional instrumentation/confirmation required.

| Chapter | What Seth owns | What ESP can analyze | Where automatic changes stop |
|---|---|---|---|
| 1. ECU configuration/testing | Build archive and map it to actual INI, sensors, I/O | Signature, configuration contradictions, telemetry validity | No hardware inference solely from config |
| 2. Trigger setup | Explain sync and independent timing reference | ECU state/sync faults, supported logs | No autonomous trigger/ignition reconfiguration |
| 3. Base tables | Identify active fuel/spark algorithms and injector baseline | Table applicability, revision, units, source | No fabricated base maps |
| 4. Base ignition/pressure | Distinguish pump controller, regulator, rail pressure, timing | Pressure-reference model, target/actual only if measured | Physical pressure/timing verification external |
| 5. Initial startup | Start-event diary and next discriminating evidence | State transition, faults, sensor validity | Never initiate a start or actuator test |
| 6. Idle | Explain air, fuel, spark, feedback and load | Stable idle patterns, target deviation, lambda correction | No unsupervised idle-loop changes |
| 7. Steady-state fuel | Resolve correct PFI strategy and bank/trim channels | Qualifying fuel observations and **review-only** VE proposals | No active VE writes in current firmware |
| 8. Steady-state spark | Explain actual spark and protective corrections | Read-only evidence and gaps | No autonomous spark advance/MBT claim |
| 9. Full-power | Organize externally supplied data | Qualified dyno data audit and comparisons | No self-directed full-power activity |
| 10. Final confirmation | Full audit and explain unresolved issues | Revision history, confidence, coverage, contradictions | No `100% safe` or `optimal` badge |

Warmup, after-start, fuel trims, transients, optional VVT/boost and other features appear as **subexperts** based on the matching INI and actual hardware. No customer is required to tune an inactive or unavailable subsystem just because it exists in a reference checklist.

## Hard gate: injection support

`UNKNOWN` => do not offer injection-specific autotune assumptions; ask injection architecture question.  
`PFI` => allow PFI-specific **read-only analysis** only when verified ECU/INI/model evidence exists; future write operations remain disabled by default.  
`GDI` or `MIXED` => `UNSUPPORTED_GDI`; block injector characterization, fuel model proposals, pressure automation, GDI actuation, and automatic calibration. Record reason; general read-only diagnostics and archive browsing remain accessible without presenting a substitute PFI path.  
`CONFLICT` between user claim and actual ECU setup => hold related experts, explain mismatch.

This restriction applies **even if a newer upstream rusEFI INI exposes GDI settings**. BrickOS has its own independent capabilities and hardware policy.

## Important fuel-system branches

**Do not equate PWM with no regulator.** Four independent dimensions:
- **Pump control:** `on_off / pwm_controller / external_controller / unknown`.
- **Pressure regulation:** `mechanical_manifold / fixed_absolute / electronic_feedback / no_known_regulator / hybrid / unknown`.
- **Pressure sensing:** `none / ECU_rail_pressure / external_verified_gauge / both / unknown`, with sensor unit/reference and validation status separate.
- **Injector reference/model:** injector's required differential pressure relationship, dead-time/flow metadata and active ECU compensation model, all separately verified.

Task selection is conditional:
- **Mechanical regulator:** do not ask for an electronic pressure-controller target unless one is actually configured; still record pressure verification and the relevant reference type.
- **PWM pump + electronic feedback + pressure sensor:** show **pressure-control strategy review** and compare configured target to reported pressure *only when validated*. A pump command is not physical pressure proof.
- **PWM pump + mechanical regulator:** both may exist. Include mechanical reference checks and a pump-control review; do not auto-delete either step.
- **PWM without a confirmed regulator or verified pressure measurement:** report `PRESSURE_CONTROL_UNVERIFIED`, block fuel-model validation rather than declaring pressure correct.
- **Independent OEM pressure controller:** preserve uncertainty about its setpoint and actual behavior; the ESP cannot infer pressure from PWM duty.
- **No sensor:** archive can still record externally reported pressure validation, but cannot assert continuous pressure tracking from ECU telemetry.

Even if a regulator is absent, **fuel pressure remains relevant to injector delivery**, and the guide must not skip physical pressure validation. rusEFI documentation distinguishes injector pressure compensation/reference types, differential/gauge/absolute sensing and PWM pump control.

## Seth negotiates the next action

He never just says `Step 4 complete`. His response packet includes:
`objective`, `why`, `necessary build questions`, `roles from matched INI`, `current evidence`, `unknowns`, `alternatives`, `execution_class`, `what can be learned automatically`, `what needs physical verification`, `manual editor link`, `conditions to revisit`, and `save/checkpoint`.

If a task depends on missing build facts, ask **one** most-informative question. If evidence is sufficient for analysis, offer `Review measurements` instead of making the user answer something the ECU can already report. If multiple causes exist, ask the next discriminating question. Record `user reported checked` separately from `externally verified`. Skipping a topic is a presentation action, not proof of completion.

The guide preserves durable `Finding`, `Task`, `Case`, `Evidence`, `EngineProfile`, `ECUDefinition`, and `CalibrationRevision` as independent versioned objects. A firmware/INI update remaps only ECU references, retains physical archive facts, and invalidates conclusions tied to obsolete offsets/table selection.

## Persistence and portability

Profile ID is **engine-specific**, not computed from a volatile INI signature. Engine profile and ECU-specific session state have distinct histories. Archive data stores a field-level `source` and `verified` status, plus optional non-sensitive reference links, with export/import and format migration. Never store credentials or secrets in the archive. On the current open ESP Wi-Fi AP, do not imply confidentiality or authentication; implement secure device pairing before permitting remote changes to sensitive archive/controller state.

Future ESP service sends resolved `brickos.guide.task.v1` and `brickos.engine.profile.v1` packets over its API so phone and later DS show the same Seth dialogue and flags without embedding ECU-specific offsets. The 3DS source and connection behavior remain unchanged until its explicit port.

## Priority and conflict rules

1. Hard capability gate first (GDI, mismatched INI, unknown firmware layout).
2. Critical ECU fault or stale/invalid sensor data blocks dependent learning.
3. Engine build conflicts block only affected experts; preserve unrelated evidence.
4. Unmet prerequisite (e.g., pressure model unknown) blocks fuel proposal but not general ECU discovery.
5. Existing independent evidence is reused; avoid redundant questions.
6. Unresolved alternatives cause an evidence request, not a numerical guess.
7. Multiple experts may observe simultaneously, but proposals and any future approved writes cannot conflict; each must be tracked against a source revision.
8. Manual Tuning must remain available and must not be automatically overwritten or hijacked.

## Acceptance tests before production implementation

- [ ] Engine profile survives INI/signature updates; no stale ECU offset is reused.
- [ ] Answering `PWM pump` does **not** infer `no regulator` or `pressure verified`.
- [ ] A controller with both PWM and mechanical regulator keeps **both** branches represented.
- [ ] No pressure sensor leaves ECU rail pressure tracking unavailable rather than zero.
- [ ] GDI/MIXED and unknown injection never enter the PFI autotune proposal path.
- [ ] A PFI INI does not override user's GDI hardware claim silently.
- [ ] Each relevant chapter asks a new question only if it changes task applicability.
- [ ] Evidence and human confirmation have distinct provenance.
- [ ] Manual editor remains unchanged; DS/DSi paths remain identical.
- [ ] Future auto-write remains disabled until explicit ECU/security/hardware validation.
- [ ] All derived outputs identify what is observed, user-provided, inferred or unknown.

## Supporting external sources (public; reviewed 2026-11-19)

- HPA public 10-step course: https://www.hpacademy.com/courses/practical-standalone-tuning/
- rusEFI Fuel Pump: https://github.com/rusefi/rusefi/wiki/Fuel-Pump
- rusEFI Fuel System: https://wiki.rusefi.com/Fuel-System/
- rusEFI Fuel Pressure: https://wiki.rusefi.com/Fuel-Pressure/
- rusEFI GDI-status: https://github.com/rusefi/rusefi/wiki/GDI-status (upstream support does not imply BrickOS support)
