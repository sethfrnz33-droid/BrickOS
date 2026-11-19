# Seth Diagnostic Agent — Master Plan V1

> **Status: proposed system architecture and requirements, not implemented ECU firmware.** Existing BrickOS Seth reasoning runs **in the phone browser**, uses a short decimated telemetry history, and cannot apply ECU changes. Current 3DS/DSi network paths and Manual Tune stay unchanged. This roadmap must not be read as evidence of completed hardware testing, engine safety, or autonomous calibration.

## Product promise

Seth, the ECU-resident clone of Brick ECU's creator, is a conversational *diagnostic case manager*. The user may enter **Full Tour**, **Help Me With a Problem**, **Assess Current Engine State**, **Analyze a Recorded Moment**, or **Review a Calibration Change**. Seth identifies the ECU-matched INI, currently active calibration strategy, available sensor readings, relevant previous events, and uncertainty before saying what he thinks. He presents **observed facts → competing explanations → the most discriminating next question or evidence source → updated assessment → report**, with short, original dialogue and optional mascot voice. The system aims to cover broad problem *classes*; it must never claim to recognize every possible mechanical failure or guarantee an optimized engine.

### The five non-negotiable user experiences

1. **Full Tour:** HP Academy's published ten-step *conceptual* order remains the default learning path: ECU configuration, trigger, base tables, ignition/fuel-pressure reference checks, startup, idle, steady fuel, steady spark, professionally instrumented power validation, and final confirmation. Warmup, transients, trims, diagnostics and optional systems are deeper expert modules reached from those chapters. The guide never performs physical engine operations.
2. **Quick Help:** A short menu (`Starting / Idle / Fuel & AFR / Sensors / Trigger / Ignition / Temperature / Connection / Advanced / Not sure`) followed by one useful question at a time. A symptom routes to the same diagnostic expert and INI-resolved field used by Full Tour.
3. **Assess This Engine:** Explain what operating state is **reported now**, identify unavailable signals, compare observed behavior with the **active ECU target and verified engine profile**, and answer `looks consistent / needs review / insufficient information` separately for each subsystem. The result is **not a safety certification or an MBT estimate**.
4. **Investigate a Moment:** Create an opt-in high-resolution diagnostic case; capture a bounded window of ECU data around an already occurring event or a qualified operator's supervised observation. Seth compares before/during/after and guides the user through explanations, not dangerous operating instructions.
5. **Follow Up:** Reopen the same case, compare independent observations and configuration revisions, mark disproven hypotheses, show a revision-linked history, export logs and evidence, or open the original Manual Tune setting. No implicit RAM write or flash save.

## Architecture: one brain, multiple front ends

```
  rusEFI ECU, native USB/INI  ->  ESP sampled observations + time metadata
                                       |
                                  Evidence recorder
                                   |     |      |
                              Case builder State resolver  Health guard
                                   |     |      |
                               Expert rules / expected-behavior models
                                            |
                              Hypothesis graph + question chooser
                                            |
                             Versioned "Seth case packet" API
                                       /          \
                            Phone browser       Future 3DS/DSi
                              dialogue          dialogue renderer
```

**No duplicated engine intelligence in UI.** The present browser analyzer is a stepping stone; production must move state classification, event recording, hypothesis results, and question selection onto the ESP so the UI update rate does not constrain measurement. The phone renders the same portable packets a future DS can display without rebuilding for each INI. The DS interface and TCP/USB protocol are **not changed** by this planning work.

## Ground truth: matched INI + calibration + physical profile

Define an immutable `EngineContext` per evaluation:
- **Verified ECU definition:** signature, firmware build, INI layout fingerprint, output channels, pages, scales, units, menu linkage and conditional availability. Unknown expressions/fields are explicitly unknown.
- **Actual active config:** engine/fuel algorithm, relevant selected tables, target lambda strategy, idle mode, cranking/ASE/WUE state, bank strategy, trim application/learning status, ignition strategy, protection flags, sensor configuration and revision.
- **User/qualified technician evidence:** independently verified engine displacement, bore/stroke, injector model, fuel, sensor model, physical timing reference and external equipment as relevant. Mark each as `provided / externally verified / unknown`; an entered number alone is not verification.
- **Environment:** ambient pressure/temperature if measured, battery/CLT/IAT, running state, load and applicable compensation. A reference envelope valid for one condition is not automatically valid for another.
- **Sources and clocks:** the timestamp and sample cadence of each value; never treat all channels as perfectly simultaneous just because they arrived in one USB response.

The INI provides **how** to interpret data; the semantic rule registry provides **what evidence means**. Do not infer a fault simply because a field is called `fuelPressure`, or assume stoichiometric is the correct commanded target in every idle/cruise/warmup condition. Compare measured lambda with the **verified active target** and disclose when the target's correctness is not established.

## Step-by-step reasoning loop

`INTENT -> DEFINITION CHECK -> STATE CLASSIFY -> BASELINE -> CAPTURE -> VALIDATE -> PATTERN -> HYPOTHESES -> DISCRIMINATOR -> EXPLANATION -> FOLLOW-UP -> REPORT`

**Intent.** Resolve the user's natural-language prompt deterministically into one or more structured topics and symptoms; detect negation, uncertainty and operating-state qualifiers. Example `coughs but won't start` becomes `starting/unstable-combustion`; it is not interpreted as evidence of a specific fault. An unrecognized prompt gets one clarifying question or category choices, never a fabricated explanation. This is a small local intent engine, not an assumed full language model running on ESP32.

**Definition check.** No matched INI/ECU identity or unresolved scale means no sensor-based conclusion or writable target.

**State classification.** Identify engine off, ECU powered, cranking reported, catch/transition, running, after-start, warming, idle state, steady operation, transient, overrun/fuel cut, fault and unknown. Primary state plus overlays (trim active, enrichment, knock intervention, closed-loop idle, etc.); use hysteresis. The rusEFI `engine` state bitmask may help *if present and correctly resolved in the matched INI*. Don't assume a simple RPM threshold proves state.

**Baseline.** Load prior verified healthy observations for the same engine context where available. Otherwise use only explicit physics/plausibility and ECU-defined targets—not invented universal healthy curves. `Expected` has a source: configured target, verified rule, externally measured baseline, independent comparable history, or `not established`.

**Capture.** Enable the appropriate event profile; capture metadata-rich samples in a bounded rolling buffer independently of phone refresh, with pre/post event context. Sample duration, actual rate, missed reads, queueing and source clock uncertainty must be retained. High-speed tooth/edge information requires ECU-supported trigger capture, **not** 100 Hz ordinary output channels.

**Validate.** Reject or degrade stale sensors, wrong units, implausible readings, unknown engine state, missing timing alignment, lambda not ready, fuel cut, compensation masking and configuration revision drift. The validity criteria are per hypothesis: a sample excluded from VE learning may remain useful for a sync problem.

**Patterns.** Compute robust descriptors of event onset, progression and recovery: changes, stable regions, bias against target, peaks, timing precedence, rate and repeatability across distinct independent occasions. Compare **qualified, comparable conditions**, not unrelated 100 Hz frames as independent trials.

**Hypotheses.** Maintain alternatives with observed support, contradictory evidence, missing sensors, physical uncertainty, and their predicted distinguishing observations. Never turn lack of contradictory evidence into proof. Fault isolation is conditional and explicitly revisable.

**Discriminator.** Select the next **safe, passive or professionally verified** measurement/question that best distinguishes the two leading explanations, subject to prerequisite severity, sensor availability, response burden, expected information gain and user-selected context. If only invasive/risky testing could resolve it, request a qualified technician's externally obtained result; do not autonomously command an engine action.

**Explain.** Seth speaks in original concise dialogue, but provides structured evidence with timestamp, units, matched field, competing causes, why a cause may not fit, what is unknown, and which original Manual Tune field is relevant. Humor stays secondary to clarity.

**Follow-up.** User may say `I checked`, `don't know`, `this got worse`, `review the log`, or provide verified external evidence. `I checked` records a reported action, **not** automatic proof. Independently confirmed evidence upgrades only the affected status.

## Proactive and requested behavior are separate

- **Passive watch:** optional lightweight read-only monitoring. It may retain anonymized/local diagnostic event summaries according to configured retention, but must not start a session or attempt an engine maneuver.
- **Question-driven capture:** user asks Seth to examine a condition; he explains which already available sensors help, arms a recorder, and waits for the **naturally occurring or professionally supervised event**. Starting/stopping a case does not start/stop the engine.
- **Full-tour checkpoint:** a stage opens the appropriate expert and can show a live case if evidence already exists; it does not force a repeat test.
- **Detected fault:** high-severity or invalid identity/sensor data can automatically flag a case and suspend *learning*, not silently apply calibration edits.
- **Autotune later:** VE evidence may contribute to offline, reviewable, bounded proposals. **Analysis and control are separate services.** Spark/boost/power changes must not be automated based on diagnostic correlations.

## Decisions and their meanings

| Output | Required meaning |
| --- | --- |
| `OBSERVED` | A source-reported reading or verified event, with context and timestamps |
| `PATTERN` | A descriptive numerical relationship within eligible measurements |
| `POSSIBLE_CAUSE` | One candidate explanation, not a confirmed failure |
| `CONTRADICTION` | A fact that weakens a specific explanation |
| `MISSING_EVIDENCE` | A field, sensor, external measurement or context not verified |
| `CHECK_REQUEST` | A passive observation, source review, or professionally performed verification |
| `REVIEW_SETTING` | Deep link to the **existing** INI-driven Manual Tuning editor |
| `HOLD` | Available evidence does not justify a calibration recommendation |
| `ESCALATE` | Qualified inspection or instrumented controlled validation required |
| `REVIEW_PROPOSAL` | Non-executable proposed calibration diff with source revision |

`OPTIMAL` is **never** inferred from current AFR, stable idle or absence of reported knock alone. Use `consistent with current target`, `stable under observed conditions`, `unverified elsewhere`, or `instrumented validation required`.

## Hypothesis-selection policy (not a magic probabilistic score)

1. **Hard failures first:** ECU identity/definition mismatch, fault and control protection, invalid critical sensor, stale data and serious communication failure.
2. **System prerequisites:** sync/reference before startup diagnosis, wideband readiness before lambda conclusion, injector and pressure assumptions before VE inference, source table identification before table attribution.
3. **User's actual symptom:** do not hijack a starting diagnosis to optimize an unrelated cruise value.
4. **Information gain:** favor an available valid channel or a safe user question that most efficiently distinguishes the remaining causes.
5. **Evidence independence:** multiple separate comparable events outrank hundreds of correlated samples in a single window.
6. **Stability:** do not reorder Seth's answer every telemetry frame. Require stable evidence and hysteresis; preserve previous hypotheses in the case timeline.
7. **No outcome guarantee:** if evidence cannot distinguish causes, say so and explain what qualified external information is needed.

## Engine-state evaluation dashboard

The `Assess Current State` report breaks into independent panels; show `not assessed` where channels, context or proof are missing.

| Subsystem | Questions Seth answers |
| --- | --- |
| Identity / sensors | Are INI and firmware matched, channels fresh and input values plausible? |
| Starting / sync | Is the current state cranking/running/unknown, with recorded sync/error trends? |
| Idle | Is idle mode actually active? Does RPM trend toward the **configured** target, and is command/feedback authority observable? |
| Fuel | What are current target vs measured lambda, sensor readiness, trim and enrichment status? Are they comparable? |
| Thermal | Does observed coolant/intake/battery behavior agree with available reference evidence? |
| Transient | Is fuel cut or acceleration enrichment active, making steady VE attribution invalid? |
| Ignition | What actual spark and correction channels are reported? What external evidence is needed? |
| Advanced systems | Which optional functions are configured, active and supported by known rules? |
| Historical | Does this behavior differ from a previous comparable and **verified** baseline? |
| Coverage | Which regions and conditions have *not* been observed? |

**Example:** At an observed idle, stoichiometric (lambda≈1) might or might not be the configured target for a specific warmed engine. Seth should compare measured/commanded and trims first, then ask whether the physical air/fuel measurement is trustworthy, rather than issue a universal idle AFR target.

## Extensible problem-case library

Each `ScenarioDefinition` names an intent and symptoms, applicable states/strategies, semantic roles, expected-source type, mutually confusable hypotheses, safe discriminators, absence/uncertainty rules, and an owning expert. Full Tour and Quick Help use exactly the same case definitions; an unfamiliar engine or INI may still be served by generic diagnostic advice without making numeric claims.

Canonical buckets:
- **ECU identity, power, network, update/INI**
- **Sensor validity and physical/electrical plausibility**
- **Cranking, catches-but-stalls, no-start, after-start and restarting**
- **Trigger synchronization, loss of phase/speed, error-counter trends**
- **Idle target deviation, hunting, stalls, load compensation, idle fuel**
- **Cold/warmup/hot-start thermal differences**
- **Steady lambda error and STFT/LTFT bank compensation**
- **Transient enrichment, deceleration, fuel-cut return, closed-loop transitions**
- **Ignition corrections/knock warnings, reported synchronization anomalies**
- **Fuel pressure / injector modeling / optional fuel systems**
- **Optional DBW, VVT, boost, flex, knock, dual-bank and CAN instruments**
- **Professional dyno, before/after comparisons and final calibration audit**

The registry must support *unknown* cases and future ECU versions without fake coverage. A test matrix must include intentionally absent channels, conflicting clues and multiple simultaneous faults.

## Seth dialogue contract

Every output packet is **source-neutral**:
```json
{
  "schema": "brickos.guide.case.v1",
  "case_id": "...",
  "ecu_definition": {"signature": "...", "ini_hash": "...", "calibration_revision": "..."},
  "mode": "targeted | full_tour | assess | event_review",
  "state": "observing | investigating | needs_evidence | review | suspended | completed",
  "prompt": {"speaker": "seth", "tone": "serious | normal | dry", "text": "..."},
  "facts": [],
  "hypotheses": [],
  "contradictions": [],
  "missing_evidence": [],
  "proposed_questions": [],
  "recommended_action": {"kind": "HOLD", "reason": "..."},
  "event_cursor": null,
  "manual_target": null,
  "provenance": {"generated_by": "expert-rule-id", "evidence_ids": [], "freshness": "unknown"},
  "read_only": true
}
```
Client controls are semantic, not firmware hardcoded: `Next`, `Why?`, `Show evidence`, `Other explanations`, `Record answer`, `Open original setting`, `Review event`, `Finish`. A future DS client renders **the packet's words**, mascot, choice IDs, and compact graph instructions; it does not implement ECU calculations or embed INI addresses.

## Concrete release plan and gates

**M30 — Rule/case registry and replay harness:** versioned taxonomy, validated semantic aliases/conditions, case contracts and synthetic scenarios for all buckets. Ship only read-only findings. **Pass:** unknown/mismatched inputs produce no invented readings and no false recommendations.

**M31 — ESP recorder:** bounded acquisition ring, event triggers, optional qualified-source trigger capture, storage budget, timestamps, missing-sample diagnostics, case persistence/replay and backpressure. **Pass:** bench stress tests with multiple Wi-Fi viewers, USB delays, record flushes and error injection; no interference with normal ECU communications.

**M32 — Operating-state and expected behavior:** runtime mode decode, target/compensation provenance, baseline comparisons and sensor plausibility. **Pass:** independent replay fixtures across different INI layouts and strategies, with no hard-coded memory offsets.

**M33 — Differential reasoning:** expert alternatives, contradicting evidence, discriminating question scoring, multi-event comparison and a case audit graph. **Pass:** intentionally ambiguous cases are explicitly labeled ambiguous; independent evidence changes hypotheses predictably.

**M34 — Phone case UI:** Ask Seth textbox/intent picker, case recorder timeline, before/during/after panels, `Why this finding`, `Compare prior event`, and exact manual links. **Pass:** mobile readability, no dangerous action prompts, manual tuner unchanged.

**M35 — ESP-owned packets / future DS compatibility:** move expert results and dialogue selection server-side with a versioned JSON schema; phone becomes a renderer, DS untouched until its later port. **Pass:** two clients see same case ID/hypotheses/INI targets.

**M36 — Professional validation and later tune integration:** separately specified, authenticated, supervised RAM proposal pipeline only after appropriate ECU-specific and bench validation. This master plan does **not** implement engine-control or ignition/power automatic actions.

## Acceptance invariants

- No inference or computation that depends on unsupported INI values.
- Explicit samples, source timestamps, uncertainty and data quality.
- High sample rate is **measured**, and no priority inversion against existing TunerStudio traffic.
- A captured moment cannot silently change engine settings.
- An untrusted/unpaired phone cannot acquire a calibration write privilege over the open Wi-Fi AP.
- Manual Tuning and 3DS/DSi communication behavior remain unchanged.
- No single global “engine optimal” indicator; per-system evidence and limitations.
- Every answer and source revision can be replayed, explained and exported.
- Unresolvable problems and unavailable sensors lead to `INSUFFICIENT_EVIDENCE`, never invented diagnosis.
- The system remains useful and accurate if a user never authorizes active learning.

## Verified references

- Current imported BrickOS source: `firmware/esp32/main/telemetry.c` (10/20/50/100 Hz requested, 4s subscriber timeout and 1024-byte output read chunks; **not** a measured guarantee), `firmware/esp32/web/guided.mjs` and `docs/29-seth-evidence-reasoning-v1.md` (browser-reasoner implementation).
- rusEFI Logging Guide: https://wiki.rusefi.com/Logging-Guide/ (100 Hz commonly useful, `engine` bitmask, trigger sync may require higher-specialized capture).
- rusEFI Trigger guide: https://github.com/rusefi/rusefi_documentation/blob/master/Trigger.md (specialized sync/signal evidence; ECU firmware/version dependent).
- HPA Practical Standalone Tuning public stages: https://www.hpacademy.com/courses/practical-standalone-tuning/ (stage progression and independent datalog validation).
