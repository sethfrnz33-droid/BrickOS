# BrickOS: Seth's Complete Teaching + Guided Calibration Master Specification

**Status:** PRODUCT/ENGINEERING DESIGN, NOT IMPLEMENTED. Updated November 19, 2026. Extends the [full system gap audit](35-full-system-gap-audit.md), [unified coordinator](33-unified-tuning-coordinator.md), [engine profile](34-engine-build-archive-and-seth-intake.md), and [event recorder](31-seth-high-rate-capture-and-evidence-spec.md). Sources inspected: current `main` firmware `telemetry.c`, `main.c`, `guide-story.mjs`, `guide-catalog.json`, ESP-IDF configuration, and HP Academy's *public* 10-step outline. No claim of physical engine readiness, automatic calibration implementation, or permission for unsupervised operation.

## 1. Product contract: three independent outcomes

Our user can arrive knowing **nothing** and should be able to learn what the engine and ECU do, navigate the matched INI, understand the specific hardware, interpret honest diagnostic evidence, and know what external validation remains before the vehicle may be considered prepared. A guide **cannot guarantee** that a novice will leave with a safely tuned/running vehicle. Mechanical, electrical and instrumented calibration verification require qualified supervision; the software must never declare broad safety from a few live gauges.

Track **three independent progress tracks**, never compress them into one seductive 100% bar:

1. **Education:** `not introduced / introduced / needs clarification / practiced / demonstrated / revisit`. A quiz, response or teach-back is **learning evidence**, not engine evidence.
2. **Configuration and hardware:** `unknown / user-reported / ECU-configured / observed / externally checked / conflict`, each field attributed to a source, method, timestamp and revision. Reading a calibration value is not physical verification.
3. **Calibration/evidence:** `not analyzed / collecting / quality insufficient / proposal ready / independently validated / stale`, **per subsystem and actual operating region**. Fuel, idle, thermal, ignition and protection progress are independent; qualified dynamometer/physical validation cannot be inferred from uninstrumented logs.

A user may finish all lessons and still have substantial engine validation outstanding. Conversely an expert may skip elementary explanations without losing access to any diagnostic or tuning feature.

## 2. Four autonomous-but-constrained components

**Seth Director** — manages intents, chapter, reader competence, questions, case history, suspense/resume, available tools and the next useful explanation. He is a narrating ECU-bound clone of the creator; humor must never conceal a fault.

**Engine Knowledge Service** — per-physical-engine archive, ECU/INI and active calibration resolver, verified semantic rules, sensor/strategy capabilities, dependencies and configuration revisions. Any `unknown` is represented, not filled in.

**Evidence & Expert Service** — independently collected and qualified telemetry, event recorder, health/fault summaries, comparison models and hypothesis results. All signal sources retain units and timestamp quality. Heuristic results disclose assumptions, alternative causes and confidence limits.

**Calibration Coordinator** — labels proposed actions as `EDUCATE`, `OBSERVE`, `USER_VERIFY`, `EXTERNAL_VERIFY`, `ANALYZE`, `REVIEW_PROPOSAL`, `UNSUPPORTED`; a distinct, security-reviewed future implementation could consider separately authorized, bounded ECU RAM actions. None of these are authorized by the current teaching design. GDI and mixed GDI/PFI remain **explicitly unsupported** for BrickOS Guided Fuel Tuning.

**Architectural invariant:** on the future production system, these components run on the ESP32-S3; the phone and later DS are *portable renderers and user input devices*, not different sources of truth or learning engines.

## 3. The user's first thirty seconds: never a blocking questionnaire

Seth asks: "Do you want the full tour, help with one issue, a review of what's happening now, or your saved project?" Then: "How familiar are you with ECUs?" Offer **New**, **Some experience**, **Comfortable**, and **Let me choose as we go**. Let the user change it at any time, globally or for one topic. If the user declines, default to a brief explanation plus an expandable lesson—never infer intelligence or expertise from mistakes.

Optional early question: "Is this a new build or an already-running vehicle?" Answer affects content sequencing but never assumes a physical engine condition. Let `I don't know`, `Skip for now`, `Why are you asking?`, and `Show the original setting` work throughout.

The engine archive is built **just in time**: questions about PFI vs GDI, pump control, pressure regulation and pressure instrumentation arise when relevant. PWM-controlled pump alone is **not** an answer to pressure regulation. An ECU/INI configuration is distinct from hardware the user has verified. Engine profile ID persists across INI revisions.

## 4. Four explanation depth modes; two axes, not a stereotype

One preference chooses **detail**:
- `quick`: 1–2 clear sentences, current finding, why it matters and a next action.
- `guided`: short explanation, one example linked to this engine and key concepts.
- `lesson`: fundamental concept, simple visual, cause/effect, common misconceptions, one low-stakes practice check and optional replay.
- `technical`: original INI field identity, native units and transformations, signal provenance, active conditions, equations or evidence calculations **when verified**, exceptions, competing hypotheses and source links.

A separate preference chooses **interaction**:
- `just_recommend`: direct finding + evidence + blockers; no compulsory quiz.
- `check_my_understanding`: optional micro-check at meaningful milestones.
- `teach_as_we_go`: Seth periodically asks if the user wants more depth and may insert a short comprehension check.
- `course_mode`: comprehensive lessons, optional examples and revisit prompts.

**Depth and interaction are independent.** An expert can request a basic analogy, a new learner can request the technical INI details, and either can skip a check. `I don't understand` triggers a different representation and an optional simpler prerequisite concept—not the same paragraph repeated louder.

## 5. The pedagogical loop, not a script maze

`LISTEN -> IDENTIFY LEARNING GOAL -> CHECK REQUIRED PRIOR CONCEPTS -> EXPLAIN -> DEMONSTRATE ON THIS CONFIGURATION OR SAFE SYNTHETIC EXAMPLE -> ASK UNDERSTANDING -> ADAPT -> OPTIONAL MICRO-CHECK -> SUMMARY -> NEXT`.

Each `LessonUnit` is a self-contained, versioned item:
- semantic `concept_id`; concise `learning_objective`; chapter links; prerequisite concept IDs; applicability and `PFI-only` safety policy;
- explanation variants for quick/guided/lesson/technical, plus plain-language analogy, comparison, diagram specification and common misconception;
- a general explanation that works offline/no ECU **plus** templates requiring verified actual ECU/INI fields; never invent live examples;
- a short optional concept check with a **reasoned answer explanation**, distractor explanations, pass/revisit status, and evidence type `education_only`;
- `if_confused` links to alternate wording, visual explanation, prerequisite or narrower question;
- `show_me_on_my_engine` with independently resolved INI semantic roles, source freshness and explicit unavailable fallback;
- `safety_limits` that prevent a learning question being treated as an operator command;
- `localization_key`, stable versions and content hash for future DS rendering.

**No educational check may command physical engine operation or modify a calibration.** Concept checks are about reasoning and source interpretation. Do not shame users or trap them in a lesson; always permit `I don't know` and return.

## 6. Understanding model: topic-specific, reversible and honest

For each concept keep separate `learner_self_report`, `optional_check_result`, `last_explanation_depth`, `last_confused_at`, `last_encountered_at`, `revisit_requested`, `content_version`. Never claim mastery from a single tap. A correct response creates `demonstrated-on-example`, not `competent to operate machinery`; a wrong response adds `needs clarification`, not a global low skill score. Support `already know this` and `show technical details` independently from progression.

Knowledge should be a **concept graph**: e.g., `pressure units -> differential injector pressure -> fuel model correction`; `ECU reported request -> physical actuator verification`; `lambda vs AFR -> target lambda -> closed-loop trim -> VE inference`; `trigger sync -> physical timing reference -> reading actual spark`; `idle target -> actual RPM -> controller response -> external load`. Different engine builds instantiate different applicable edges; GDI remains an unsupported branch.

Questions are diagnostic of *understanding*, never of personal worth. Seth may ask `Still with me?` after one dense explanation or repeated `Explain simpler` requests, but not on every dialogue screen; respect `Don't ask me again for this chapter`.

## 7. Full Tour = HPA public 10-step conceptual order, with learning, hardware and ECU states

For each chapter instantiate the same five panels:
**Understand this**, **My engine configuration**, **What the ECU reports**, **What needs checking**, **What Seth can analyze**.
A chapter has a one-line objective and clear exit condition. The owner may browse any chapter without falsifying its status.

| Stage | Key teachable concepts | External proof not inferable from ECU |
| --- | --- | --- |
| 1. Configuration / I-O | ECU vs engine, INI identity, sensor inputs, outputs, source truth | Actual hardware assignments and physical installation |
| 2. Trigger/sync | Crank/cam purpose, engine phase, sync errors, timing reference | Physical trigger/timing validation |
| 3. Base fuel & ignition model | Fuel strategy, target vs measured, table axes, injector model | Actual installed hardware and fuel supply characteristics |
| 4. Reference checks | Ignition reference vs commanded spark, pressure reference, pump vs regulator | Timing reference and pressure checks with suitable external methods |
| 5. Startup observations | crank/catch/after-start, sensor readiness, unknown causes | Safe commissioning by qualified operator; no autonomous startup |
| 6. Idle | target vs actual, airflow, fuel, spark, feedback and mechanical loads | Physical actuator/system correctness |
| 7. Steady fueling | lambda target, trims, VE, independent observations, transient exclusions | True fuel-system behavior/measurement accuracy |
| 8. Steady ignition | actual advance vs base table, corrections, knock limitations | Instrumented torque/knock validation |
| 9. Higher-load validation | independent dyno evidence, repeatability, protection system context | Qualified controlled-facility verification; no high-load self-testing |
| 10. Confirm and document | audits, unresolved cases, source revisions, unobserved regions | Appropriate external safety/vehicle checks and final professional signoff where needed |

Warmup, after-start, closed-loop trims, pressure models, DBW, VVT, boost, flex fuel, transient fueling, cooling/oil protections and optional CAN devices become relevant expert branches. The user need not learn or calibrate unsupported hardware.

## 8. The conversation manager really negotiates

Every Seth response offers **one primary task**, with `Why?`, `Explain simpler`, `Teach the full lesson`, `Give me the short recommendation`, `Show measured evidence`, `Try a concept question`, `I don't know`, `Skip explanation`, `Open original INI setting` and `Back` as context-appropriate options. On a blocked stage, Seth explicitly says whether the obstacle is knowledge, missing build facts, missing ECU field, stale sensor, unsupported strategy, or an independent physical check; no decorative generic `Next` that implies a false pass.

**"I don't get it":** Seth identifies which concept is unclear, changes wording/format once, optionally starts its prerequisite lesson, then reasks a different conceptual question **only when the user opts in**. He preserves the original engine case and returns there afterward. `I'm lost` returns to a concise map: `where we are`, `why we're here`, `what I already know`, `what I still need`.

**"Just do it":** Seth says which requested task can currently be automatically *observed/analyzed* and which are manual/externally verified, with action scope and unsupported claims. Dialogue alone is not write permission. If the user asks whether the car is safe to drive, report unresolved validations and defer to appropriate qualified assessment rather than certify it.

**"Why did you recommend that?":** give provenance IDs and plain-language chain `observed -> rules -> alternative interpretations -> confidence -> missing evidence -> next safe check`. Explanations are readable without a raw dump but offer expanders for technical detail.

## 9. Engineering boundaries: ESP-only intelligence, no embedded generative model dependency

A reliable self-contained ESP32-S3 build uses a **deterministic expert system** with static versioned text/lesson bundles, a small structured intent recognizer, a per-user concept-state graph, INI semantic resolver, task planner and cached diagnostic case summaries. No network LLM/API, no runtime training, no unbounded prompt evaluation, and no claims of open-domain intelligence.

The phone uses a generic browser renderer delivered by the ESP. The future 3DS/DSi receives compact text/screen/action packets; display vocabulary and voice animation are clients, not control logic. A changed generated INI can change active bindings without recompiling the client; semantic rule gaps still require **verified rule-pack updates**, not guessed memory addresses.

### Build resource reality from current repository

The current target `sdkconfig.defaults` selects a **4 MB ESP32-S3**; `partitions.csv` allocates:
- NVS: `0x6000` = 24 KiB (not a general-purpose lesson database);
- factory app: `0x280000` = 2,621,440 bytes;
- SPIFFS `logs`: `0x170000` = 1,507,328 bytes, **already shared with existing logging**.

An 80–100 KB dialogue JSON file and embedded portrait/audio consume flash in the **firmware partition** when compiled; source C size is not the binary size. There is currently **no dedicated pedagogy/profile partition**, and these allocations leave no OTA app slot. Do **not** assume PSRAM, SD card, onboard ECU NAND, writable free SPIFFS or sufficient spare binary size. Measure actual ELF/map image size, free heap (including contiguous largest block), PSRAM presence and storage throughput on supported boards before committing to a content bundle.

**Design choices:** compile a compressed lesson index into const flash or load a versioned read-only bundle from validated storage; stream/decode one small node at a time, bound all strings, use numeric compact role IDs and immutable shared content; keep only current session, small concept state, active INI metadata and bounded recent case summaries in RAM. Defer large waveform storage to verified physical media; no unbounded browser request bodies or JSON tree of the entire manual.

### Scheduling and failure isolation

Existing USB/ECU, TunerStudio TCP, manual write/burn, real-time data sampling and web dashboard retain precedence; Seth's explanation renderer and quizzes are low priority. Never generate teaching text by blocking a 100 Hz telemetry task; any high-rate recorder is a separate bounded producer/consumer. The phone refresh rate cannot pace ECU acquisition. If memory/time quota is exhausted, degrade `technical -> lesson -> brief static text`, omit graphics/audio first, suspend recorder analysis second, **never** degrade ECU command integrity or essential fault reporting.

Archive progress with versioned, checksummed, wear-aware bounded records. Prefer checkpointed append/atomic replace and recovery without wiping calibration. On full storage, keep the source evidence and honest `not saved` status. Audio and portrait are optional. Every teaching route remains usable with no device connected: explanatory mode, no invented live values.

## 10. Threat model and physical safety

The current AP is open and not a trustworthy operator identity. **Before ESP-owned personal progress or engine archives are remotely editable, design pairing, authorization, read-only spectator access, replay protection and consent/retention/export.** A phone disconnect suspends any active future controller lease, not a passive education page. Reconnection never automatically resumes write privileges. Store no private identifiers unnecessarily.

Never:
- trigger a starting event, run an actuator test, alter spark/boost/fuel/VE, command high-load experiments or burn calibration merely because a lesson/diagnostic step was completed;
- conflate `I learned it`, `I checked it`, `ECU reported it` and `qualified validation completed`;
- treat absence of knock, static AFR, MAP or accelerometer data as evidence of MBT or broad safety;
- provide GDI-specific tuning or silently use PFI fuel assumptions for GDI/mixed builds;
- bypass ECU hardware/firmware protections in the name of tutoring.

## 11. User-facing definition of success

`Course complete`: all chosen educational concepts introduced or intentionally skipped with topic-specific status, optional learning checks and a reusable lesson index.
`Configuration reviewed`: applicable configuration/INI fields audited and hardware facts labeled verified vs user-reported.
`Calibration evidence available`: each relevant operating region has provenance-tagged measurements and validation limitations.
`Vehicle/engine validated`: only the externally qualified reviews actually documented; Seth **never** issues an unqualified `vehicle is safe` verdict.

The final personalized report must separate all four. It explains **what the learner understands**, **what the software observed**, **what still cannot be established**, and **what qualified steps remain**; it is a durable teaching and diagnostic handover, not a guarantee of roadworthiness.

## 12. Acceptance gates / non-negotiable regression tests

1. Any user can enter with **no engine connected** and complete introductory lessons; no fake sensors appear.
2. Each learner can select `quick` and `lesson` independently from `check understanding`; change mid-chapter without losing the case.
3. `I don't understand` produces a different presentation or prerequisite; never repeats identical text or auto-marks competence.
4. A wrong or skipped concept check changes only learning state, **not** hardware verification or ECU readiness.
5. `I checked` stays user-reported; confirmation needs independent evidence recorded separately.
6. A changed INI retains engine facts and learning progress while invalidating dependent ECU-specific conclusions.
7. PFI/PWM with unknown regulator asks about regulation, not declares absent. GDI/mixed never enables fuel autorecommendation.
8. Every displayed live value comes from verified active INI role + fresh source; no missing sensor shown as 0.
9. All HPA chapters can launch contextually relevant lessons; Quick Help invokes same expert and concept IDs.
10. The same ESP-authored packet renders on phone and later console; unsupported UI media falls back to plain text.
11. Under memory, storage, Wi-Fi and USB faults the guide degrades or pauses, never blocks ECU communication or silently corrupts an archive.
12. The default observer can't command ECU control. Any future write controller needs a separate validated security and transaction implementation.
13. No learner progress or external task status produces `engine safe` or `100% optimized` from heuristics.
14. Run end-to-end simulation including novice learner, uncertain engine profile, mismatched INI, sensor fault, re-entering a case, course restart, storage full and interrupted update.
15. User can request complete source explanations, diagrams, glossary, quiz retry, `I don't know` or direct recommendation from any lesson without triggering a new engine operation.

## 13. Recommended delivery order

- **A**: Release truth manifest & tutorial data schema; preserve original manual and console paths.
- **B**: Tiny ESP-executable deterministic lesson/player and educational state policy, with two languages of output (short + detailed) and 3DS-compatible packets; initial offline prototype & tests first.
- **C**: Bounded, authenticated local profile/progress persistence and INI semantic bindings.
- **D**: Live evidence case recorder + engine diagnostic/task coordinator integrated with lesson launch points.
- **E**: Fully designed phone UI (lesson cards, question prompts, show-me-evidence, course map, state assessment, case review).
- **F**: PFI review-only VE model and structured proposals after tests/strategy detection; no automatic ECU write yet.
- **G**: Qualified-controlled, separately reviewed RAM tuning capability only after dedicated firmware/security/hardware signoff (not authorized by this specification).
- **H**: DS port consumes the same packet contract; no hardcoded dialogue or fixed INI pages.

## Public context

- HP Academy public ten-step sequence, not copied proprietary curriculum: https://www.hpacademy.com/courses/practical-standalone-tuning/getting-started-the-hpa-10-step-process/
- ESP-IDF ESP32-S3 flash partition reference: https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32s3/api-guides/partition-tables.html
- Existing repo verified sources: `firmware/esp32/partitions.csv`, `sdkconfig.defaults`, `main/telemetry.c`, `web/guide-catalog.json`, `docs/35-full-system-gap-audit.md`.
