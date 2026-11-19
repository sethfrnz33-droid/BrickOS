# Seth Diagnostic Cases — Phone UX, Questions, and Portable Conversations V1

> **Design documentation only.** This is a proposed evolution of the existing Ask Seth browser UI. The diagnostic case packet, continuous recorder and scenario registry are not currently executed by ESP firmware. Current native DS/DSi connection endpoints and Manual Tuning stay untouched.

## The five ways to enter Seth

**Home** is a dedicated, entirely optional Guide workspace with large readable cards:
- `Take me through the whole tune` — the existing HP Academy-ordered 10-chapter Full Tour.
- `Something isn't working` — symptom-driven diagnosis with nested category questions.
- `How is it running right now?` — current-state health *assessment* for available sensors, without declaring the engine safe or optimal.
- `Investigate a moment` — read-only event capture/review, displaying real rate and capture capabilities.
- `My previous cases` — event diary, comparisons, follow-up answers and exported evidence.

The user can always return to `Manual Tuning`; Guided does not replace or block INI navigation. The phone does not become a high-rate telemetry acquisition engine. The ESP will eventually own the reasoning; meanwhile the current browser implementation must clearly label its capabilities as limited.

## Core layout — phone portrait

```
┌─────────────────────────────────────────┐
│ BRICKOS     Ask Seth         ECU: MATCH │
│ Mode: Guide   Last data: LIVE or STALE │
├─────────────────────────────────────────┤
│ [ pixel Seth portrait: upper / jaw ]   │
│ ┌─────────────────────────────────────┐ │
│ │ SETH                                │ │
│ │ "What are we figuring out today?"  │ │
│ │ [typed text / optional voice]       │ │
│ └─────────────────────────────────────┘ │
│ [Full tour]       [Troubleshoot]        │
│ [Assess state]    [Review moment]       │
│ [Open past cases]                      │
├─────────────────────────────────────────┤
│ LIVE SOURCE  INI MAPPED / NO ECU       │
│ 10-stage progress | 2 open cases       │
│ [Manual Tune]  [Evidence] [History]    │
└─────────────────────────────────────────┘
```

The portrait talks with the user-supplied nonverbal voice and jaw animation. The text is authoritative for comprehension, not the sound. The existing **lore** stays occasional: Seth is a somewhat exasperated copy of the creator stuck in the ECU. No joke overrides fault details or missing evidence.

## Case creation workflow

`Describe -> Confirm interpretation -> Choose available evidence -> Observe/review -> Compare alternative explanations -> Ask next discriminating question -> Summarize -> Preserve case`.

1. **Describe:** user can type a symptom, choose a category, or select an existing ECU event. The text parser identifies intent/topic/operating state plus negation and uncertainty. It does not invent a physical fault or assume an LLM is present.
2. **Confirm:** Seth briefly states how he interpreted the problem and offers `That's it / Not quite / I'm not sure` to reduce incorrect routing.
3. **Source preview:** show the relevant INI-resolved live channels, **missing** channels and estimated recorder coverage. If no ECU attached, offer general textbook explanation only, without a live diagnosis.
4. **Observe:** user explicitly starts a **recording-only** case; Seth passively observes existing or professionally supervised events. `Arm recorder` never controls the engine; there are no automatic actuator or ignition tests.
5. **Explain:** top finding is a combination of facts, uncertainties and *multiple* supported hypotheses. The first hypothesis may be marked `more consistent with this evidence`, not `proven cause`.
6. **Ask:** a short next question chosen from sensor availability and information gain. `I don't know` is always valid; questions requiring physical measurements are labeled for an appropriately qualified person.
7. **Review:** source plots, time windows before/during/after, source units, ECU config revision, missing frames and reasons certain evidence was rejected.
8. **Save case:** preserve the original samples and annotations with source/provenance and clear storage limitations. The report contains no direct ECU control action.

## The case detail page

```
┌─────────────────────────────────────────┐
│ CASE: Starting irregularity      OPEN  │
│ Source: matched ECU / capture PARTIAL  │
│ event marker: reported state change    │
├─────────────────────────────────────────┤
│ SETH                                  │
│ "I caught a change in the reported   │
│ synchronization state. That matters; │
│ it doesn't prove what's wrong yet."  │
├─────────────────────────────────────────┤
│ FACTS [2]         ALTERNATIVES [4]     │
│ ● ECU reports a sync-status change    │
│ ● Voltage evidence: unavailable       │
│ 1. Signal / sync interpretation       │
│ 2. Electrical reference uncertainty   │
│ 3. Fuel / ignition hardware issue     │
│ 4. Mechanical cause still possible    │
├─────────────────────────────────────────┤
│ [WHAT CHANGED?] [SHOW THE TRACE]       │
│ [WHY THIS?]    [OTHER EXPLANATIONS]    │
│ [WHAT'S MISSING?] [SAVE CASE]          │
├─────────────────────────────────────────┤
│ [OPEN ORIGINAL SETTING] [BACK]         │
└─────────────────────────────────────────┘
```

Example is illustrative, not output from an actual connected ECU. Missing values explicitly display `unavailable`, not 0. A high-severity ECU fault is emphasized without Seth initiating physical engine operations.

## Full Tour + targeted help use identical case experts

The ten-chapter tour gives Seth the learning sequence; diagnostic cases can be entered from **any** chapter without resetting progress. The phone stores a `return_to` breadcrumb for the chapter and check question. A case that arises from a manual setting routes back to the same original INI-defined menu/field.

Examples:
- `Step 2 / Trigger` invokes the exact same `trigger_error_increments` scenario as `Quick Help > Trigger`.
- `Step 6 / Idle` invokes the exact same `idle_hunts` scenario as `Quick Help > Idle > Wanders`.
- `Step 7 / Fuel` invokes `fuel_trim_masking` if the native closed-loop correction flags are supported and indicate activity, but **never** assumes target lambda is universally stoichiometric.
- `Step 10 / Confirm` produces an evidence audit with unverified regions and explicit professional-validation requirements, not a "100% tuned" award.

## Exact behavior for “How is the engine running right now?”

One neutral state tile: `RUNNING REPORTED / CRANKING REPORTED / OFF / UNKNOWN`, plus **source freshness**. Then separate panels: `Sensors`, `Idle`, `Fuel / Target Lambda`, `Fuel Feedback`, `Ignition / Knock`, `Thermal`, `Protections`, `Recent Events` and `Unknown/Not observed`.

Each panel has:
- **Observed:** timestamped source values, units and active state.
- **Compared against:** configured ECU target, verified manufacturer/source rule, same-engine comparable baseline, or `no verified reference`.
- **Finding:** `consistent with observed target`, `not consistent`, `data quality issue`, or `insufficient evidence`.
- **Caveat:** context-dependent fuel targets, fuel-trim masking, lambda delay, external engine load and physical health cannot be assumed away.
- **Next useful data:** one read-only follow-up observation or a qualified external verification if needed.

Seth cannot claim `optimal timing`, `optimal VE`, `safe to run`, or `safe to drive` from short ordinary telemetric snapshots.

## Dialogue examples (original Seth voice; not HPA content)

### Starting complaint with ambiguous evidence

Seth: "That start sounded enthusiastic in the worst possible way. Let me see what the ECU actually reported."
User: `Show what happened`.
Seth: "I can tell you whether it reported speed and sync, and whether the data stayed fresh. I cannot tell you that the injectors or coils physically did what the ECU requested."
User: `What could cause it?`.
Seth: "There are several possibilities: timing/sync information, delivery hardware, or a mechanical issue. I'll keep them separate until the evidence points somewhere useful."
User: `What's missing?`.
Seth: "A verified physical reference or an ECU-supported trigger trace may be needed. I won't fill that blank with a guess."

### Idle wandering (with data quality labels)

Seth: "Idle isn't just an RPM number. I'm comparing the target, actual speed, actuator request, spark changes and fuel feedback—where this ECU exposes them."
User: `Did the actuator cause it?`.
Seth: "Maybe, but cause and response aren't the same thing. If RPM changes first and the actuator follows, that changes what we'd investigate next."
User: `Can you fix it?`.
Seth: "I can explain the evidence and take you to the exact INI setting. I can't promise a calibration fix for a mechanical or wiring issue."

### Fuel target and trims

Seth: "The measured lambda is close to the commanded target in this observation. The closed-loop system may be helping, though."
User: `So the VE map is good?`.
Seth: "Not necessarily. I need to know the active correction state and how this firmware scales the trims. A nice number on a gauge isn't the whole story."

### Current-state assessment

User: `Is it optimal right now?`.
Seth: "I can review what this ECU reports for its current state and explain what fits the configured targets. I can't establish the best ignition timing or all operating conditions from one snapshot."
User: `What do you need next?`.
Seth: "A trustworthy baseline and comparable evidence for the missing regions. I'll show exactly what hasn't been verified."

## Dialogue control rules and character tone

- Seth communicates **small statements** followed by 2–5 useful response choices, plus `Explain more`, `Show source`, `I don't know`, `Back` and `Open original setting`.
- Questions are selected from the **same verified scenario registry** used in phone/DS portable packets. The phone does not embed ECU offsets or independently decide whether a sensor is faulty.
- A detected fault uses sober direct text, not comic improvisation.
- The portrait and optional voice do not block visual information and can be muted. Speech does not determine runtime engine behavior.
- A user's typed free-form description is untrusted **context**, not sensor proof. The parser preserves raw text but never directly translates it into ECU commands.
- The agent does **not** initiate engine starts, trigger simulated faults, command I/O or adjust the tune as a way to answer its own questions.

## Prompt intent parser (offline-first)

Proposed pipeline:
1. Normalize case/spelling and strip delimiters while preserving negation (`won't start` vs `starts`), chronology (`after warming`) and state (`idle` vs `cruise`).
2. Classify coarse topic from a **versioned synonym list** and optionally select a known registry scenario.
3. Ask only the highest-information **missing detail** that can be provided by the user without assuming a risky operation.
4. Resolve available INI roles; missing roles constrain—not cancel—general explanation.
5. Use the current recorded case and prior evidence to select a portable dialogue packet.
6. Unknown language gets selectable topics and `Sorry, I don't have enough to classify that` instead of fake language-model certainty.

Avoid treating each word as a diagnostic. For example `crank` could mean crank sensor, cranking event or a mechanic's phrase; prompt disambiguation is preferred when no state clue resolves it.

## Cross-device wire contract

The ESP serves portable, bounded packets with `schema`, `case_id`, `dialogue_id`, `speaker`, `text`, `tone`, `choices`, `ecu_source_revision`, `source_quality`, `facts`, `hypotheses`, `required_evidence`, `manual_target`, `plot_spec`, `event_cursor`, and `status`. The phone renders the first-class layout; a future DS receives the same text and IDs and may render a simplified timeline/table rather than long graphs. All pagination and waveform downsampling are on the ESP service.

**No arbitrary browser-generated unsafe ECU commands**, even when Seth appears to be holding an interactive conversation. Shared API permissions remain separate from read-only case creation and history.

## Verification and regression tests

- Opening Quick Help never changes the Manual Tune route tree.
- Exact same case ID and evidence after entering from Full Tour or Quick Help.
- Missing INI sensor does not appear as a fabricated zero or default healthy value.
- Unknown speech is clarified rather than forced into the wrong category.
- Case can be reopened after phone refresh if persistent storage exists.
- A new INI signature or calibration revision invalidates affected case conclusions without deleting source history.
- Annotated before/during/after chart marks sample gaps and uncertain timing.
- Many correlated frames do not imply many independent events.
- No physically unsafe test suggestions are automatically generated.
- A pending/expired session cannot authorize automatic ECU action.
- Future 3DS text rendering consumes the same dialogue ID/choices without recompiling language/INI semantics.
- Every source value has a unit, freshness and reason when unknown.

## Related sources

- `docs/30-seth-diagnostic-agent-master-plan.md`: full engineering and product strategy.
- `docs/31-seth-high-rate-capture-and-evidence-spec.md`: recorder timing, buffering and safety.
- `design/diagnostics/scenario-registry.v1.json`: 57 **design-only** sample diagnostic case definitions, covering 13 areas.
- `docs/25-portable-ini-guide-contract.md`: original cross-device dialogue principles.
