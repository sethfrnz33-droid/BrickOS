# BrickOS Implementation Readiness: What Must Be Resolved Before Writing the Agent

**Design and source-research pass:** November 19, 2026. **No firmware, browser runtime or 3DS transport changed in this pass.** This is a traceable integration plan, not a proposal to perform unsupervised vehicle testing or to guarantee a tuned vehicle. It extends [40](40-verified-external-source-ledger-and-rusefi-constraints.md), [36](36-seth-complete-teaching-and-tuning-master-spec.md), [35](35-full-system-gap-audit.md), the [role candidate ledger](../design/research/source-role-candidates.v1.json), and the [130-concept inventory](../design/education/curriculum-coverage.v1.json).

## A. One coherent product and one truthful source of capability

The architecture has two user entry points, **Full Guided Tour** and **Get Help Now**, sharing an **ESP-owned engine archive**, **verified matching-INI interpretation**, **diagnostic evidence**, and **education/agent task graph**. An optional `Assess Current ECU` is the same reasoning engine with no user-selected symptom. Seth's creator-clone backstory, dialogue text, voice and portrait are presentation features; they do not change physical safety, authorization or source certainty.

A `Capability` is an immutable record of:
- firmware/build and target hardware revision;
- ECU signature + generated INI hash + page list;
- associated engine-profile revision, if user opted in;
- feature ID and **supported / unsupported / unknown / disabled / physically unverified** status;
- exact semantic role map, required data-quality checks, valid operating states and applicable active strategy;
- authoring/validation provenance of the expert rule;
- observation / lesson / diagnosis / review-only proposal permission, **never implicitly calibration-write**.

A new INI *may* update field locations, scales, menu links, table dimensions and available readings without updating any phone/DS app. A **novel algorithm** or non-equivalent field meaning requires a human-reviewed semantic rule update on the ESP; dynamic INI parsing must not invent expertise.

## B. The four kinds of question Seth may ask

1. **Learning question:** "Do you understand what target lambda means?" → updates *education only*, not engine condition.
2. **Engine-build question:** "What fuel delivery arrangement is physically installed?" → user-reported physical archive; may be `unknown`.
3. **Definition question:** "Which fuel model is enabled?" → automatically read from matching ECU/INI **only when the exact option is supported**; do not ask users to guess something already known.
4. **Evidence/validation question:** "Was the physical reference independently checked, and is that evidence available?" → user report or qualified external result with provenance; never an automatic hardware pass.

Every stage separates these questions. The chosen lesson depth affects words and examples only. `I don't get it` routes through the concept graph, not directly to an ECU command. `I don't know` is always valid.

## C. Ten stages × action ownership

| Stage | Seth's learning objective | What code can know automatically | What remains human/qualified | Action policy |
| --- | --- | --- | --- | --- |
| 1. ECU configuration | Meaning of inputs, outputs, physical vs configured hardware | Signature, generated-INI metadata, fault bits and known config | Correct wiring and actual installed hardware | Read-only discovery |
| 2. Trigger | Crank/cam phase and meaning of sync status | Reported sync/error counters; special trace only if supported | Physical trigger installation/reference confirmation | Read-only diagnosis |
| 3. Base tables | Active fuel model, injector characterization, table axes/corrections | Verified active config and table metadata | Correct injector/pressure/fuel facts | Read-only assessment |
| 4. Reference checks | Command versus physical timing/pressure | Configured control modes, pressure channels **if validated** | Independent physical references | Document/hold |
| 5. Starting | Reported engine-state sequence, sensor readiness | State bitmask and qualified recorded event | Qualified supervised commissioning | Analyze archived event only |
| 6. Idle | Feedback, target vs actual, competing causes | Idle status and validated, timestamped channels | External actuator/load/hardware inspection | Passive analysis |
| 7. Steady fuel | Lambda/target/trim interactions and evidence eligibility | Active PFI strategy, matched target/measurement and validity | Correct physical fuel model and measurement quality | Review-only proposal after verified model |
| 8. Ignition | Base vs actual spark, warning evidence | Matched ignition/correction channels | Independent torque/timing evidence | No automatic spark advancement |
| 9. Professional validation | Which measurements qualify and why | Import/review trusted external evidence and revision | Controlled facility/instrumented validation by qualified operator | No engine test commands |
| 10. Confirm and document | What was learned vs what was measured | Config/version/event history; evidence scope; missing checks | Required qualified external signoff, vehicle inspection | Audit/report only |

The coordinator must not assume reaching a chapter means the engine physically passed previous checks. Users may freely **read** any lesson without changing applicability or authority; dependent calibration conclusions remain on hold until evidence exists.

## D. Critical INI implementation decisions

### D1. Parser breadth
The current browser parser handles common constants/output channels and some menu definitions, **not a guaranteed complete TunerStudio grammar**. Before moving expert logic to ESP, build a parsing-feature matrix covering generated pages, macros, expressions, conditional settings, enum/bit fields, dynamic-sized arrays, multi-bank data, lookup-bound axes, display-only aliases, physical command buttons, nested dialogs, hidden labels/indicators, and unknown constructs. Reject unsupported or out-of-bounds definitions, not replace them with generic fallbacks.

### D2. Canonical field identity
Use a typed key `{ECU-layout-hash, actual-page-identifier, byte-offset, width, type, semantic-role, scaling-context, active-condition, display-alias-group}`. This key is valid only for the captured layout; migrations remap through verified new definitions. Two display fields pointing to the same bytes share **one canonical storage object**. An `info` label or static indicator never becomes an editable list item.

### D3. Status and physical evidence
The `engine` log channel can contain multiple state bits simultaneously; do not compress into a single `idle/running` boolean. Sensor validity and physical correctness are different. A configured pressure sensor that is missing or unwired must not be rendered as healthy measured pressure, and an enabled ECU strategy must not be inferred solely from an INI section existing.

### D4. Multiple source clocks
Timestamp each ECU read request and completion, optional ECU-native event time and external measurement. A single snapshot can contain values captured at different internal moments. Evidence requiring precise chronological order is **unsupported** when the clock uncertainty exceeds the observed effect.

### D5. Manual editor links
Never make a deep link by fuzzy first-match label. Use the exact menu tree/dialog/table origin from the matched INI, plus a stable field identity, visibility predicate and source revision; if unresolved, show a link to the closest confirmed category **labeled as non-exact**, not an incorrect direct field action.

## E. Persistent archive decisions requiring hardware and security proof

The current 4 MB target includes a single factory app and a logs partition. No safe capacity is established for a growing evidence database. Before implementation:
- measure actual flash/app/heap, free SPIFFS, PSRAM/SD status and worst-case memory while Wi-Fi, USB and current manual app are active;
- decide if engine archive + learner checkpoints fit in **small, infrequent** NVS transactions, or require a separate verified partition with migration;
- adopt checksummed, versioned, recoverable archive records with per-field provenance and an explicit `not saved` warning if storage fails;
- never repurpose `logs` or change the partition layout in place without a migration and recovery plan;
- on the currently open AP, do not accept engine archive edits as authenticated identity. Establish pairing/role permissions before multi-user persistent changes, and **separate tutorial sessions from any future manual ECU write privilege**.

## F. Diagnostic agent design: cases before conclusions

A `Case` is independent of a lesson and contains: the user's original symptom text, resolved intent/category (possibly unknown), selected engine profile and ECU/INI revision, event times/source types, validity flags, observations, alternative explanations, contradicting evidence, safe next information source, unresolved evidence and narrative response. **No single-cause certainty** from limited measurements.

When Seth prompts for event evidence, the future ESP can passively mark a bounded already occurring or professionally supervised condition. Do not command starting, throttle, actuators, boost, ignition, road maneuvers or high-load operation to obtain a sample. Trigger tooth-level capture is special firmware-dependent data and is not reconstructed by faster ordinary polling.

The agent's loop is: **interpret intent -> check sources/context -> find comparable evidence -> state multiple supported hypotheses -> choose most discriminating safe information -> explain -> archive -> rerun when better evidence exists**. 
`No evidence` produces educational help or a qualified external handoff; it does not generate a tune number.

## G. Autotune ownership must be separate from Seth's words

Keep the PFI VE analytics pipeline independent from presentation and education. It must be **supported only for an active, matching, validated PFI fuel strategy** and reject GDI/mixed/unknown hardware, unsafe or invalid source states, unresolved correction semantics, configuration conflicts, sensor delay beyond quality limits and absent physical fuel-model evidence.

Differentiate:
- **Observe:** general logs and state data;
- **Analyze:** qualify windows and compute uncertainty/banked corrections without changing ECU;
- **Review candidate:** explain affected table region, original/recommended *hypothesis* and why;
- **Future apply:** distinct, professionally reviewed, authenticated/calibration-versioned, narrowly bounded RAM transaction **only after hardware-specific verification**;
- **Separate save:** explicit independent ECU flash persistence with verified protocol/rollback semantics.

A command loss or non-acknowledgment must never be represented as successful RAM/flash operation. The current guide is read-only.

## H. Learning design work that remains before declaring "complete textbook"

The initial 130-topic curriculum is a **coverage plan**; 108 concepts still lack four authored explanation variants, alternative analogy, misconception-specific feedback and optional check. The original 22 seed lessons are useful fixtures, not a finished course. Do not simply manufacture 108 generic texts from a template and claim coverage; prioritize novice prerequisites and review by a subject expert.

Minimum authoring validation for each concept:
- language and scientific correctness, explanation at four depths, practical **non-operational** example, and source-specific applicability;
- distinct `I don't get it` alternative, optional quiz with **shuffled correct choice** and explanation of every distractor;
- chapter/dependency links, glossary cross references, relevant verified INI roles, no-device/offline fallback;
- translation/localization capacity on future DS, text legibility and voice-independent clarity;
- education-only completion status and explicitly separate external/physical verification.

HPA's public course order supplies the framework, but Seth's original content must be written, reviewed and testable independently. The ESP dialogue engine should not require a cloud model.

## I. Concrete project integration phases (planning first)

| Phase | Outcome we can test | Required artifacts | Blocker to next |
| --- | --- | --- | --- |
| P0 | Capability truth in current repo | firmware manifest, source ledger, reproducible matching INI fixture, exact build status | Cannot assert supported channels otherwise |
| P1 | INI semantic parser, no hardware actions | role-binding fixtures, canonical field identity, conditional expression test set | Unknown scaling/active strategy |
| P2 | ESP teaching core | bounded C lesson reader, catalog compiler, response packet API, host/hardware heap benchmark | Binary size/stack/latency unresolved |
| P3 | Engine and learner archive | owner selection, versioning, independent provenance, power-loss tests, secure pairing decisions | Open AP/flash conflict |
| P4 | Case recorder and state classification | timestamped bounded ring, source-type distinctions, case export/replay and actual rate metrics | ECU USB contention/unverified sources |
| P5 | Seth decision coordinator | HPA stage engine, concept prerequisites, hypothesis/question graph, portable phone packets | Unsupported command or source ambiguity |
| P6 | Phone browser experience | full Seth dialogue, per-stage task statuses, saved return position, manual deep link, current assessment | Misleading progress or stale data |
| P7 | Review-only PFI fuel advisor | eligible evidence, active lookup, feedback-state handling, independent visits and explainable proposal | No validated matching PFI model |
| P8 | Later console rendering | versioned packet consumption only; current 3DS/DSi transport/editor unchanged | Client-dependent INI logic |
| P9 | **Separate future decision** | write authorization/security transaction protocol, bench/hardware validation or explicit no-write product policy | No assumptions about safe live operation |

**No integration into runtime until phase-specific measured evidence exists.** A passing JSON schema test is not a physical ECU validation.

## J. Open decisions that the present repo cannot settle by research alone

| Question | Why it matters | What evidence will settle it |
| --- | --- | --- |
| Which *exact* generated INI is the product intended to support first? | Field identity, active strategies and protocol differ by build | Matched ECU signature + complete generated INI + tune snapshot |
| How much flash and PSRAM does the actual ESP32-S3 board have? | Lesson/catalog and persistent archive fit | Device reports, boot logs, board/PCB datasheet, built app size |
| Is the selected ECU logging medium available through the current USB bridge? | Raw event storage may differ substantially | Verified ECU firmware and bus/protocol/SD evidence |
| Which sensors and actuator drivers are *physically* installed on each user's engine? | Profile branching and confidence | Build archive + independently validated hardware records |
| Who can authorize shared profile edits and any future calibration session? | Current Wi-Fi AP is open | Explicit product identity/pairing/security design and validation |
| Which active/running modes expose reliable bank-specific STFT/LTFT? | Prevent feedback masking errors | Matching INI, output-channel semantics and representative logs |
| How are manual TunerStudio edits detected during a guided session? | Prevent stale recommendations and conflicting writes | ECU revision/readback strategy and concurrency tests |
| What is the measured sustained USB data rate and storage headroom? | High-rate recorder schedule | Bench rate/latency/overrun profiling with actual hardware |
| How will rules be maintained when rusEFI introduces new strategies? | Avoid generic string-matching diagnoses | Verified semantic rule pack + migration/compatibility policy |
| Who signs off the educational accuracy and physical validation criteria? | Beginner trust and claims | Documented editorial and professional review process |

If any of these remain unknown, Seth must **say so** rather than generate a fictitious calibration value.

## K. Release acceptance

No `all-in-one tuning agent` release is complete until:
1. All ten stages have accessible novice-to-expert lessons, optional understanding checks, revisit logic and one-question-at-a-time guidance.
2. Every relevant INI field is accounted for; unsupported semantics explicitly reported.
3. Engine profile is durable, independent of the ECU identity, and never promoted from self-report to physical proof.
4. All live source facts have units, freshness, context, fault/status flags and provenance.
5. Diagnostic cases describe alternatives/counterevidence rather than unsupported certainty.
6. Phone and DS share the same server-authored case/lesson semantics; no client-specific INI offsets.
7. Profiler/SD/heap and ECU USB throughput meet measured margins, and failures degrade noncritical UI first.
8. GDI/mixed remain blocked, and **no educational step grants ECU calibration writes**.
9. No result claims a novice or engine is physically safe, roadworthy or fully optimized solely from GUI/telemetry.
10. Independent professional/hardware validation is plainly identified wherever required.

**Review status:** architecture and public facts researched; target-specific ECU field captures, firmware integration, production hardware testing, content expansion and independent qualified signoff are *not* complete.
