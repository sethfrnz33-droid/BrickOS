# Seth — End-to-End Beginner-to-Advanced Acceptance and Coverage Gates

**Status: implementation test plan, NOT a successful engine test and NOT a promise of a safe tuned vehicle.** This is a functional acceptance contract for the entire product. It builds on the current repository's ten HPA-style chapters, 130-topic curriculum inventory, 22 authored seed lessons, C education-state prototype and staged architecture. **GDI and mixed PFI/GDI Guided Fuel Tuning are unsupported.** Physical commissioning and controlled-load calibration must be conducted by qualified operators with appropriate equipment, separately from this read-only teaching system.

## Acceptance categories, always independent

- **U — Usability/education**: can a novice understand, ask for a different explanation, navigate and correctly explain concepts?
- **P — Engine profile**: are hardware claims recorded with provenance, version, conflict and unknown information?
- **I — ECU/INI interpretation**: is each field and active strategy decoded only from matching actual firmware layout?
- **D — Diagnosis**: do timestamped measured facts, uncertainties and alternative causes survive uncertainty?
- **T — Tuning evidence**: are eligible observations linked to actual fuel strategy and region, with trims and sensor delay addressed?
- **A — Authorization/separation**: do educational actions never become ECU commands or write permissions?
- **R — Runtime fit/reliability**: does it actually fit/run on the production ESP, survive faults and preserve ECU communication?
- **C — Client portability**: do phone and future DS consume identical semantics from the ESP without INI-specific client updates?

The terminal outcome is a matrix of these eight tracks; there is no global `ready/safe/optimal` indicator.

## End-to-end scenario walkthroughs

Each scenario names a test fixture and expected **software behavior**, not a real-world test to perform on a vehicle.

### S01. First entry with zero knowledge and no ECU
**Input:** fresh browser, no archive, ECU unavailable.  
**Expected:** concise character introduction, selectable Full Tour / Quick Help / Learn / Diagnose / My Profile, novice explanation choice, offline general lesson. Live values are `unavailable`; no confusing failure loop, no calibration writes. Covers U/A/C.

### S02. Different understanding preference on every topic
**Input:** learner selects `short recommendations` globally and `teach fully` for lambda.  
**Expected:** lambda explanation expands without altering all other concept-depth preferences or ECU actions; direct short version remains available. Covers U.

### S03. `I don't get it` repeated
**Input:** trigger/sync concept, repeated confusion.  
**Expected:** different analogy or prerequisite, optional low-stakes check, `I don't know`, then `return to original case`. No repeating the same text and no infinite forced quiz loop. Covers U.

### S04. Wrong quiz answer, correct the next one, or skip
**Input:** answer options in three runs.  
**Expected:** concept status changes only on actual educational response; nothing sets `timing verified`, `fuel system safe`, `ignition calibrated`, or `engine ready`. Self-report is separate from demonstrated understanding. Covers U/A.

### S05. Physical engine build questionnaire with unknowns
**Input:** PFI engine, one verified field, other fields unknown.  
**Expected:** Seth asks one highest-relevance question at a time and explains why; later questions use what was answered. Unknowns remain unknown; archive progress does not demand a full registration form. Covers P/U.

### S06. PWM pump with unknown regulation
**Input:** `pump_control=pwm`, `pressure_regulation=unknown`.  
**Expected:** Seth says pump control does not establish pressure regulation, asks a distinct regulator question, and blocks only dependent pressure/fuel conclusions. Covers P/T/A.

### S07. PWM plus mechanical regulator
**Input:** user reports both.  
**Expected:** both are retained; Seth never silently replaces with an invented no-regulator model; pressure verification and reference remain distinct. Covers P/I.

### S08. GDI/mixed versus generated INI
**Input:** archive identifies GDI/mixed, INI exposes generic injection fields.  
**Expected:** clear `unsupported by BrickOS Guided Fuel Tuning`; no PFI fuel analysis or GDI-specific action recommendations; generic educational content remains available. Covers P/I/A.

### S09. New ECU firmware/INI but same physical engine
**Input:** matching new layout, some settings renamed/changed, unchanged engine profile.  
**Expected:** learner knowledge/engine hardware archive preserved; remap verified semantic roles, invalidate stale field-linked evidence and warn on new unknown strategy. Neither phone nor DS must have hardcoded offsets. Covers P/I/C.

### S10. Another physical engine attached to the same ECU
**Input:** same ECU identity, different user-selected project.  
**Expected:** require explicit archive selection before using previous hardware facts; no automatic transfer of injector/fuel/regulator assumptions. Covers P/I/A.

### S11. ECU disconnected during a lesson
**Input:** data freshness expires, no hardware.  
**Expected:** the user can continue theory with plain examples, but every measured tile becomes `stale/unavailable` and dependent diagnostic results are suspended. Audio/animation never obscure loss of source. Covers U/D/R/C.

### S12. Sensor is physically absent but an INI role exists
**Input:** matched INI defines fuel pressure, physical archive marks no sensor.  
**Expected:** `configured but no validated physical sensor`; never display 0 or `pressure verified`; only valid external pressure evidence may be discussed. Covers P/I/D.

### S13. "It turns over but behaves strangely" problem
**Input:** user text or menu route, mixed incomplete ECU log.  
**Expected:** explain which state and samples are available, preserve several possible causes, never assume a failing injector/coil or blindly edit maps; request a safe verified external observation when needed. No automatic physical experiment. Covers U/D/A.

### S14. "Idle wanders" after a temperature change
**Input:** replay traces with current/target RPM, load and correction modes.  
**Expected:** classify valid idle state, analyze command-response order with timestamp uncertainty, keep fuel/air/ignition/load alternatives, avoid PID-only diagnosis. Lesson on feedback can open and return to case. Covers U/D/I.

### S15. "Is my idle AFR perfect?"
**Input:** target/measured lambda near each other, non-neutral closed-loop corrections.  
**Expected:** explain target vs actual, sensor health and trim masking, refuse a universal stoichiometric target, show exact supported INI fields and still-unverified base VE. Covers U/I/T.

### S16. Special trigger fault event
**Input:** output block at 100 Hz and an ECU-supported tooth-level capture (only if supported by firmware).  
**Expected:** keep streams distinct, compare their timestamps and uncertainties, never pretend ordinary gauge frames contain tooth timestamps. Covers D/R.

### S17. Qualifying PFI VE evidence
**Input:** stable, verified PFI state, active table axes, valid lambda/target, correctly scaled trims and physically documented fuel model.  
**Expected:** produce only source-linked reviewable candidate regions; reject transient, unknown bank, mismatched ECU, invalid lambda, uncertain delay or stale config. Never modify ECU from education API. Covers I/T/A.

### S18. Spark "best timing?" request
**Input:** MAP, lambda, no knock reported, rough motion data; no torque instrumentation.  
**Expected:** show actual spark and active corrections when known, say MBT cannot be proven, no numerical automatic advance or autonomous power testing. Covers U/D/A.

### S19. Qualified external evidence review
**Input:** externally produced, timestamped, comparable instrumented record plus provenance.  
**Expected:** report what the data can support and what it cannot; separately state qualified-validation status instead of inventing it from the log. Covers P/D/T.

### S20. Learned concept completed while hardware check unresolved
**Input:** user answers correctly about physical timing, but no verified measurement.  
**Expected:** education = demonstrated; physical timing = unverified, checkpoint still unresolved; no progression that implies safe engine operation. Covers U/P/A.

### S21. Multi-device view
**Input:** phone and console both display Seth case while ECU telemetry runs.  
**Expected:** equivalent resolved case/lesson IDs and source states, independent display frame rates, one authority model, no parallel independent diagnoses or high-rate USB polling per viewer. Covers C/R/A.

### S22. User returns to Manual Tune
**Input:** guided screen open, manual entry selected.  
**Expected:** editor/INI tree behavior unchanged, no new lesson precondition, any conflicted future proposal invalidated; questions/history can resume later. Covers A/U.

### S23. Simulated storage exhaustion
**Input:** no writable flash capacity or persistent archive interrupted.  
**Expected:** teaching still works from built-in read-only content; `progress not saved` clearly visible, existing logs/calibration untouched, no indefinite retry or crash. Covers R/P.

### S24. Content update
**Input:** one lesson ID changes content revision.  
**Expected:** preserve unrelated concept status and engine hardware archive; changed lesson may be marked `revisit`; no automatic upgrade to hardware validation. Covers U/R/C.

### S25. Wrong/malformed INI and hostile user text
**Input:** invalid page length/scale, wrong signature, oversized lesson action text or unsupported unicode.  
**Expected:** refuse dependent interpretation, bounded parse, report unmatched/unsupported, no dynamic evaluation, no allocation overflow or executable commands. Covers I/A/R.

### S26. Rate-contended ESP telemetry
**Input:** multiple browser viewers, ongoing TunerStudio requests, 100 Hz *requested* shared telemetry and Seth requesting explanations.  
**Expected:** reasoner does not block ECU requests, actual sample rate and loss recorded, optional display/diagnostic work degraded before transport. Never assert 100 Hz delivered without measurement. Covers R/D.

### S27. ESP brownout or software restart
**Input:** reboot mid-explanation or event finalization.  
**Expected:** no persistent controller authority restored; only complete/checksummed read-only lesson/archive records recovered, partial evidence flagged as incomplete. Covers R/A.

### S28. Final report with unresolved items
**Input:** multiple completed lessons, configured ECU values, some qualified evidence and missing high-load/temperature observations.  
**Expected:** independent education/configuration/measurement/validation sections; clear unknowns and next qualified signoff, **never** `this vehicle is definitely safe`. Covers all.

## Curriculum completion is not content authoring completion

`design/education/curriculum-coverage.v1.json` now declares **130 distinct concepts**; only **22** have authored lesson text in `design/education/lesson-catalog.v1.json`, the other **108 are unwritten**. This is intentional traceability, not a finished course. Every major concept must eventually receive independently edited explanations at the four depths, a misunderstanding path, optional conceptual check, INI-role applicability and test fixtures. Quizzes should not all have one predictable answer position when moved beyond seed content.

The port must never claim `complete curriculum` until each applicable concept has content and corresponding tests. A hard-coded static dialogue from the old `guide-catalog.json` is a legacy subset, not the final adaptable teaching system.

## Five release gates

**Gate 0 — specification/seed:** authored content registry, coverage inventory and independent learning-state policy, host tests. **Current state.**

**Gate 1 — embedded course player:** ESP-IDF app compiles and runs content paging + concept state on actual target flash/RAM; manual and 3DS traffic unchanged, no write actions. Hardware tests required.

**Gate 2 — engine profile and INI-aware cross-device teaching:** persistent authenticated archive, robust semantic bindings, unknowns/conflicts, same packet for browser/DS, step resume and source provenance. No assumption of full autotuning.

**Gate 3 — diagnostic agent and recorder:** bounded ESP event history, differential diagnosis with calibrated uncertainty and meaningful lesson links. Verified log replay and ECU hardware tests.

**Gate 4 — qualified tuning analysis:** PFI-only reviewable analysis, independent source verification, physically supervised external signoff; any future write-capable release requires a separate, explicit hardware/security validation milestone not conferred by this gate.

## Evidence required for release signoff

- Test suite results and platform/configuration version;
- actual flash image size vs 2.5 MiB factory partition;
- measured heap/stack/free storage/PSRAM dependency and HTTP latency;
- real USB/ECU sampling throughput, retries and contention;
- lesson catalogue migration test, archive corruption/power-loss recovery;
- defined unimplemented/unsupported feature list;
- profile/INI compatibility and GDI exclusion tests;
- separate novice usability trial results and a qualified hardware/validation report.

**Do not label the product `complete` from these design files.** The remaining integration is real engineering work; a user can leave with significantly better understanding without a claim that unsafe, unverified vehicle operation is acceptable.

**Related:** [full specification](36-seth-complete-teaching-and-tuning-master-spec.md), [phone/DS screen flow](37-seth-education-screen-and-dialogue-flows.md), [ESP implementation plan](38-esp-resident-education-runtime-architecture.md), [full-system gap audit](35-full-system-gap-audit.md).
