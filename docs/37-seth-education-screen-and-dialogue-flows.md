# Seth Learning UX — Full Phone Conversation and Future DS Screen Contract

**Status: design only.** This is a screen plan, not a claim that the current phone app includes the educational system. `design/education/lesson-catalog.v1.json` is a design/example source; `prototype/esp_education_core.c` is a pure-C state example, **not** yet linked into the ESP application. Full engine commissioning and physical testing remain externally and professionally supervised. This specification never authorizes starting, loading or adjusting an engine.

## 0. Guide is a separate optional app, not an overlay on Manual Tune

Global header `Manual | Ask Seth`; choose the guide without disturbing the TunerStudio/INI tree, gauges, manual editor or logged files. Header shows `ECU connected/unknown`, `engine profile project alias`, `observations live/stale`, and `Seth paused/observing`. Do not display a confusing `100% tuned` meter.

**Home entry cards (all explicit):**
1. **Walk me through everything** — ten-chapter conceptual route based on HP Academy's public outline.
2. **Help with one problem** — starting/idle/fuel/sensors/trigger/ignition/warmup/connection/other topic router, one useful question at a time.
3. **How is this engine doing?** — per-system, source-labeled *evidence* report; never a universal optimality or safety assertion.
4. **Teach me a concept** — independent lessons and glossary; usable with no ECU.
5. **Review a captured event** — passive history, data quality, competing causes, source timestamps, no engine-triggering procedure.
6. **My engine profile** — hardware facts, unknowns, provenance, PCM/INI association and unresolved contradictions.

In every route, Seth can ask whether the person wants a short answer, a full lesson or a technical explanation; the choice does not change privileges or engine state.

## 1. Friendly onboarding — only one short interaction

Seth: "I'm Seth. Technically a clone of the guy who made this ECU. I'm stuck in here, so I might as well get your engine explained properly. Have you dealt with EFI tuning before?"
- `I'm brand new` → preference `guided + teach_as_we_go`.
- `I know the basics` → `guided + optional_checks`.
- `I already tune engines` → `quick + just_recommend`.
- `Let me decide as I go` → `guided + optional_checks`, no permanent skill label.

Seth: "You can change this any time. It only changes how much I explain—not what the ECU does." Then show home. Avoid a survey and avoid judging the learner's intelligence from a self-description.

### What a novice sees

```
┌──────────────────────────────────────────┐
│ BRICKOS / ASK SETH      ECU: NOT READY   │
│ PROJECT: none           [Profile ▾]      │
├──────────────────────────────────────────┤
│   [pixel Seth, talking mouth animation]  │
│ SETH: "What do you want to understand?"  │
│ ┌──────────────────────────────────────┐ │
│ │ [Full guided tour]                   │ │
│ │ [I need help with something]         │ │
│ │ [Explain a concept]                  │ │
│ │ [What does the ECU know?]            │ │
│ └──────────────────────────────────────┘ │
│ Learning: preferences saved locally      │
│ ECU verification: not started            │
├──────────────────────────────────────────┤
│ MANUAL | EVIDENCE | HISTORY              │
└──────────────────────────────────────────┘
```

**Important:** a student without a connected ECU can study diagrams and simulated examples. The Guide must label them `EXAMPLE`, not `LIVE`.

## 2. Four independently selectable explanation depths

### Quick answer
Seth: "PWM is how the pump is commanded. It doesn't establish whether a regulator exists. That's a different hardware question."

Below: `Show why`, `Show related INI`, `Get full lesson`, `Not sure what PWM means`.

### Guided
Seth: "A pump command and fuel pressure regulation are separate things. Your build archive says PWM pump; we haven't established whether it uses a mechanical regulator, electronic feedback, or something else. I need that part clarified before treating fuel pressure as verified."

Below: `What is a regulator?`, `What can the ECU measure?`, `I don't know`, `Save this question`.

### Full lesson
An explanation in 3–5 **progressive cards**, not a scroll of 1,000 words:
1. `What is being controlled?` — basic pump control concept.
2. `What is being regulated?` — pressure architecture and why it is distinct.
3. `How would we know?` — measured vs commanded, without physical test instructions.
4. `My ECU / My build` — which INI fields and hardware facts are verified, unknown or contradictory.
5. `One optional understanding check` — short conceptual reasoning with answer feedback.

The lesson can use a simple diagram or read-only animation. Never use a missing signal as a numerical example from this vehicle.

### Technical
Show the same ideas plus the verified ECU-specific semantic field names, units, scaling and active mode, provenance references, limitations, signal timing and relevant source documentation. **Technical depth isn't a more dangerous test mode**; controls are unchanged.

## 3. "I don't get it" is a first-class state transition

Every substantial explanation offers `I'm lost / Explain another way` and `Still following?` at **natural boundaries**, not every few seconds.

1. The device sends `action=confused` with `concept_id` and `lesson_version`.
2. ESP records `NEEDS_HELP` only for that concept; it does not mark the person globally inexperienced or change engine readiness.
3. Seth offers a shorter analogy, a diagram, or a prerequisite mini-lesson (e.g., `before injector differential pressure, what do we mean by a reference pressure?`).
4. Allow `I get it now` as self-report, a *voluntary* alternative concept question, or `Not now`. Do not force a pass.
5. When returning to the engine case, show `We were checking pressure-source confidence for your fuel model`, not an abrupt jump to a random chapter.

The first fallback must be **different content**, not an exact repetition. If the second attempt remains unclear, offer another mode or a small glossary/concept map. The guide must not endlessly repeat a lesson or pressure the user into affirming understanding.

## 4. Educational checks: useful but never a physical-verification gate

After completing a concept, Seth may ask:

> "Quick understanding check: PWM controls pump output. Does that prove a mechanical regulator is absent?"

Choices: `No` / `Yes` / `I'm not sure`.
- `No` → "Right: control and regulation are separate questions. That still doesn't tell us what hardware is on *this* engine."
- `Yes` → "Not quite. A PWM-controlled pump can also have a mechanical regulator. Want to see the diagram that separates those?"
- `I'm not sure` → "That's fine; we can review the comparison or carry on with the uncertainty recorded."

A correct quiz answer records `DEMONSTRATED_ON_EXAMPLE`, never `fuel system verified`, `ignition reference correct` or `vehicle safe`.

Other optional conceptual checks: `is lambda target the same as measured?`, `are ECU commands proof of actuator operation?`, `does no knock establish MBT?`, `can a warm-idle observation certify cold starts?`. Never ask the user to perform a physical maneuver to answer an educational check.

## 5. Learning screen layout — phone portrait and landscape

Portrait:
```
┌────────────────────────────────────────────┐
│ SETH / LESSON 2 OF 5      ⌄ Detail: GUIDED │
│ ECU: CONNECTED            MODE: READ ONLY  │
├────────────────────────────────────────────┤
│ [pixel Seth]                               │
│ "A target is different from a measurement"│
│ [typed text with optional voice + jaw]    │
│ ┌────────────────────────────────────────┐ │
│ │ Target: configured value (if known)    │ │
│ │ Measured: source-verified value        │ │
│ │ Validity: source health / freshness    │ │
│ └────────────────────────────────────────┘ │
│ [Explain simpler] [Show a diagram]         │
│ [Explain technically] [Try a question]     │
│ [Show my ECU] [Skip explanation]            │
│ [Back to case]                             │
├────────────────────────────────────────────┤
│ Education: introduced / needs help        │
│ Physical verification: unresolved         │
└────────────────────────────────────────────┘
```

Landscape/tablet may show Seth/lesson to the left and relevant INI/source evidence to the right. Never display an unsafe giant confirmation CTA. A gauge in a lesson is a **verified, timestamped observation** or clearly labeled fictional illustration.

## 6. Two-screen DS port, no second brain

**Upper screen:** Seth's portrait, current question/explanation, concise diagram or logged evidence and `data live/stale` labels. **Lower touchscreen:** four or fewer large answer buttons, `Explain simpler`, `More detail`, `Show source`, `Back` and `Open Manual`. Long technical content paginates through packets; the DS never infers a sensor value or INI offset from text.

The same `lesson_id`, `concept_id`, `provenance`, `action IDs`, `check answer` and `progress status` originate from ESP. No audio means Seth still shows meaningful text. The existing console TCP/USB connection paths remain unchanged until a later explicit integration release.

## 7. A beginner's complete cross-mode journey

**Part A — curiosity:** no ECU connected. Learns `ECU versus engine`, `what a sensor reports`, and `target versus observed` at their chosen depth. A skipped check remains untested.

**Part B — build archive:** user names a local project; Seth gathers relevant hardware facts progressively, starting with injection architecture when fueling is involved. PFI may enter read-only fuel analysis; GDI/mixed explicitly cannot. Unknown pressure regulation blocks pressure-dependent claims, not learning.

**Part C — source discovery:** ECU connects, signature and INI match, source units resolved. Seth compares *configured* settings with *reported* hardware without overwriting either. Sensor channels are `observed` only when they are actually fresh and decoded.

**Part D — full HPA-ordered tour:** each chapter has `understand`, `my engine`, `ECU evidence`, `what to verify`, and `possible passive analysis`. A qualified physical check is a distinct handoff with explicit follow-up provenance, never a button that certifies a hardware condition.

**Part E — symptom investigation:** user types "idle wanders" or selects the Idle expert. The agent opens the same case whether reached via Full Tour or Quick Help, records available evidence and asks the most informative supported question. User taps `Explain what PID means` and returns to the case after the lesson.

**Part F — evidence-based review:** source-timestamped logs, no fake readings, active trim semantics and competing causes; unsupported data yields `insufficient evidence`. If a reviewed PFI VE proposal ever becomes available, presenting it is **not** authorization to change the ECU.

**Part G — final report:** separate cards for learning, hardware/configuration, observed calibration scope and professionally supplied evidence. Unverified operating regions stay visible. `Finished the guide` never equates to `the vehicle is safely tuned`.

## 8. Crucial edge cases the dialogue system must handle

- A user chooses `expert` at onboarding but later requests a complete beginner lesson: respect the local request without resetting their other preferences.
- A user repeatedly answers incorrectly: try alternate examples and no-pressure help, not a locked progression or claims of dangerous incompetence.
- A user says `I checked it` but no external evidence exists: record `user-reported`, not `verified`.
- The engine is still cranking or the ECU is disconnected: do not show stale lambda as a running mixture.
- A different INI is loaded during a lesson: preserve the concept, rebind any live evidence, and warn when an illustration is no longer applicable.
- The attached engine is a different physical engine: ask to select the correct archive, never silently transfer hardware facts.
- A lesson has no relevant active INI field: offer a general explanation with `this ECU doesn't expose that value`.
- The lesson catalogue updates mid-progress: retain historical completion but mark affected concept version as `review available`.
- Wi-Fi drops mid-sentence: stop the speech animation, show offline/stale status, preserve learning checkpoint when authorized, and never pretend an ECU command was executed.
- The device has no audio or stereo: all facts, warnings, questions and choices must remain available through text.
- Multiple devices watch: they can render the same lesson/case; only an authenticated owner can edit shared saved learning/profile state.
- The 4MB flash target can't store an optional diagram: omit that diagram, not the required text or caution.
- User asks `is it safe to run or drive now?`: report only validated facts and ask for qualified inspection; no blanket guarantee.
- User requests GDI tuning: clearly state unsupported; explain the scope limit without substituting a PFI sequence.
- A red/high-severity fault is present: direct diagnostic text supersedes mascot humor, never a surprise lesson or a background automatic adjustment.

## 9. Required UI components

- `SethDialogue` speaker/title/text/typewriter/voice/mute, with plain instant-render fallback.
- `DepthControl` quick/guided/full lesson/technical; `InteractionControl` no checks/optional/teach-as-you-go/course mode.
- `ConceptCard` original objective and alternate explanation, glossary links, relevant verified roles.
- `ConceptCheck` low-stakes question, options, targeted misconception feedback, `I don't know`.
- `CourseMap` 10 HPA chapters, independent educational/hardware/evidence status and deep links.
- `EngineArchive` fact-by-fact source and verification, conflicts, version/export and privacy/consent.
- `CaseInspector` facts, competing hypotheses, missing evidence, event trace, manual deep link.
- `LiveEvidenceTile` source, unit, valid range provenance, timestamp/freshness, unknowns, no fake healthy badges.
- `Handoff` user/qualified check reason, what information is needed, return point, no automatic actuation.
- `FinalReport` distinct learning/configuration/evidence/external validation sections.

## 10. Release tests

1. Both explanation axes persist without controlling ECU session state.
2. An incorrect concept check changes **only** the educational record.
3. A new INI changes field links but leaves general learning intact.
4. The same case and concept deliver equivalent semantics to phone and DS packet consumers.
5. Any physical validation task uses independent source categories and can be left unresolved.
6. No dialogue path can trigger start, ignition changes, boost, fuel writes, flash burn or engine maneuvers.
7. Screen remains understandable without audio, JavaScript animation or connection.
8. Returning from a prerequisite lesson restores the original symptom/case.
9. No lesson is marked complete or unverified engine condition silently certified merely by pressing Next.
10. All safety and source-trust messages remain visible at every depth.

**Related files:** [Master specification](36-seth-complete-teaching-and-tuning-master-spec.md), [Lesson catalog](../design/education/lesson-catalog.v1.json), [Portable contract](25-portable-ini-guide-contract.md).
