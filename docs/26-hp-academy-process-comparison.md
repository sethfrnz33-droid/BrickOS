# HP Academy workflow comparison — BrickOS Guided Tuning design review

**Date reviewed:** 2026-11-19  
**Scope:** Publicly available HP Academy course syllabus and publicly accessible lesson descriptions/transcript excerpts. This does **not** imply access to all paid course material, endorsement by HP Academy, or permission to reproduce their lessons. The analysis and Seth dialogue suggestions below are original BrickOS design proposals, not HP Academy course content.

## Public-source baseline

High Performance Academy's published *Practical Standalone Tuning* workflow uses a ten-step process:

| Published stage, summarized | BrickOS/Seth responsibility | Design status/gap |
|---|---|---|
| 1. ECU configuration and testing | Verify INI/signature, ECU engine strategy, input/output wiring assumptions and sensor validity | Existing read-only inventory is partial; hardware checks must be explicit |
| 2. Trigger setup | Check sync status, trigger assumptions and physically verified ignition reference | Existing trigger expert is advisory, no physical confirmation automation |
| 3. Base table configuration | Review usable baseline fuel/spark/compensation configuration and correct active strategies | **Missing clear first-class Guided baseline stage** |
| 4. Base ignition timing / fuel pressure | Record verified timing-reference and fuel-system configuration evidence | **Missing dedicated gate with explicit human/equipment verification** |
| 5. Initial startup | Record crank/catch/after-start events with source/provenance and conditions | Proposed event diary only, not fully deployed |
| 6. Idle tuning | Explain air, fuel, ignition correction, actuator state, target and feedback separately | Existing guide contains a placeholder expert; needs richer state analysis |
| 7. Steady-state fuel tuning | Evaluate matched INI fuel model, target vs measured lambda and correct trim handling | Browser collects evidence; no production ESP resident full VE advisor |
| 8. Steady-state ignition tuning | Spark strategy audit and explanation of missing torque-quality evidence | Smart Spark is read-only; must not claim MBT from knock absence or MAP |
| 9. Full-power tuning | Link only to professionally supervised instrumented validation and evidence review | Not an autonomous BrickOS operating or driving procedure |
| 10. Confirm tune | Final review of changes, unresolved faults, evidence scope, logging and external confirmation | Existing final-review stage is a concept, not validated tune certification |

Source: HP Academy public 10-step list and commentary:
- https://www.hpacademy.com/blog/haltech-tuning-online-training-course-high-performance-academy/
- https://www.hpacademy.com/courses/practical-standalone-tuning/
- https://www.hpacademy.com/courses/road-tuning/

The public *Road Tuning* curriculum additionally calls out **completing the tune**, **transient fueling** and a **final checklist** after its major stages; maintain these as separate review tasks rather than letting a nominal 10-step wrapper hide them. HP Academy also foregrounds practical measurement tools, input/output testing and datalogging. We reviewed the public listings, **not** a proprietary course implementation or private curriculum.

## What this comparison changes in BrickOS

### 1. Full Tour becomes a disciplined sequence, without hardcoding a universal ECU
Use the HPA-like **conceptual progression** as a dependency order, but dynamically instantiate each chapter from the matched rusEFI INI and active ECU configuration. A stage's title and conversational intention may be shared; underlying field paths, enabled status, explanations, units, evidence and links must come from the current matching definition and verified semantic rule registry.

Target navigation:
`INTRO -> ECU DEFINITION -> ENGINE / I-O VERIFICATION -> TRIGGER -> BASELINE CONFIG -> PHYSICAL REFERENCE CHECK -> START EVENTS -> IDLE -> WARMUP -> STEADY FUEL -> FUEL CORRECTIONS -> TRANSIENTS -> IGNITION EVIDENCE -> EXTERNAL VALIDATION -> FINAL AUDIT`.

The sequence is a **learning path**, not an automatic command queue. Supported stages may be explored out of order in read-only mode. No physical engine operation is triggered by tutorial progression.

### 2. Add two explicit prerequisite chapters
- **BASELINE / PREPARATION:** verify active fuel and spark models, source table identities, axis scaling, injector assumptions, engine protection configuration and known calibration history. The system distinguishes *recognized value* from *externally verified correctness*. Do not invent a base map.
- **REFERENCE CHECKS:** show the reported timing reference/fuel pressure channels if present, explain that ECU-reported values cannot independently prove physical sensor or mechanical accuracy, and require a recorded external verification result where necessary. No fabricated pass/fail from a sensor channel alone.

### 3. Make the learning state visibly separate from the dialogue
Every guide chapter gets status `DISCOVERED / NEEDS_SETUP / NEEDS_MEASUREMENT / NEEDS_EXTERNAL_CHECK / OBSERVING / REVIEWED / BLOCKED`. Seth may narrate why something matters, but the ESP's machine-readable evidence record determines stage status; pressing **Next** never marks an unresolved dependency complete.

### 4. Full Tour and Targeted Help share the same underlying expert/task model
The full tour selects the next prerequisite by dependency order. Targeted Help selects a symptom category (Fuel, Starting, Idle, Sensors, Trigger/Sync, Ignition, ECU/Connection, Warmup, Advanced), then asks relevant questions and returns to **exactly the same expert and INI-derived field target**. This prevents maintaining a separate copy of tuning logic for each mode.

### 5. More helpful than a generic course is evidence-aware interaction
Seth's dialogue should answer:
1. **What we know:** ECU/INI-derived fields and reported live measurements.
2. **What we don't know:** missing sensors, mechanical/physical verification, unavailable data or unsupported firmware variant.
3. **Why the present stage matters:** dependency and resulting uncertainty.
4. **Where to look:** exact INI-native entry in Manual Tuning, retaining manual UI independence.
5. **What changes the finding:** accepted evidence, required external confirmation, or detected config revision.
6. **What not to infer:** a running engine isn't proof its configuration is correct; target lambda under closed loop isn't proof VE is correct; absence of reported knock isn't proof optimal ignition timing.

### 6. Treat warmup and transients as review modules, even when the main tour is brief
- The main tour can group these under **Finishing / Behavior**, but preserves deep-linkable separate modules for start-by-temperature diary, after-start/warmup corrections, lambda-readiness and transient behavior.
- Never conflate transients with VE or idle sensor faults.

### 7. The guide should ask fewer questions than a video course
INI discovery should automatically answer active strategies, tables, units and hardware assignments *when reliably available*; ask a user only for information an ECU cannot establish (e.g. externally verified setup, actual physical sensor/model, external measurement availability). A user must be able to say **I don't know** without being led to guess or bypass a blocker. The full tour maintains the exact unresolved question.

### 8. Visual design and narrative
Seth's phone guide remains an original, softly humorous **ECU-bound clone** character with text and optional nonverbal voice/portrait. The functional display is precise and not roleplayed engineering authority. Use short dialogue pages, `Explain more`, `Why?`, `Check data`, `Open INI setting`, `I don't know`, `Back` and `Not now`. Ensure the entire text/catalogue/state contract is delivered by ESP to future 3DS/DSi clients, not compiled into each UI.

**Original sample Seth dialogue (not taken from HPA):**
> "Before we get lost in fuel tables, I'm checking what this ECU actually thinks it's controlling. I've been stuck in here long enough to know a number isn't the same as a verified wire."

> "I can read the ignition setting. I can't reach out of the ECU and prove the physical reference matches it. This one needs an external check."

> "Idle is doing something strange. It could be airflow, fuel, control feedback or something mechanical. I'll show you what the data can support; we aren't guessing at a PID value."

> "Lambda looks tidy, but fuel feedback is helping. Nice to know. Not enough to call the underlying VE table correct."

> "We're done with this explanation, not with validating the engine. Those are two separate achievements, and I'd rather not pretend otherwise."

### 9. Capabilities that must be measured, not advertised as already implemented
- Complete INI/page/conditional semantic resolution across representative rusEFI variants.
- Correct rule ownership and dependency graph, especially baseline and mechanical reference.
- Durable evidence history with source revision and physically verified state separate from ECU-derived state.
- Robust communication/session handling independent of phone UI frame rate.
- Full end-to-end deterministic replay across 10-stage conceptual journey plus optional experts.
- Browser usability on actual phones and later 3DS dialogue portability.
- Hardware-based ECU validation before claiming any real-world calibration feature.

## Proposed development acceptance checks
- Full Tour exposes clear baseline and reference stages before start/idle reviews.
- Targeted Help for Idle/Fuel/Trigger routes to the same exact INI field and expert state as Full Tour.
- Missing target lambda, unavailable pressure sensor or invalid INI gives **missing evidence** rather than a guessed answer.
- A different rusEFI generated INI changes page/field targets without changing the phone/DS dialogue engine binary.
- Existing Manual Tuning tree/editing and 3DS network protocols remain unchanged.
- No guide step initiates engine starting, actuator testing, dynamometer operation, ignition advance or calibration save on its own.
- Completion report names validated regions and unresolved limitations rather than a false `100% tuned`.

**Product direction:** use the strengths of HPA's progressive *ordering* and foundational checks, then exceed a static tutorial through real ECU discovery, contextual explanations and explicit uncertainty. The system does not copy, redistribute or claim to replace HPA's paid instruction.
