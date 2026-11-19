# BrickOS Comprehensive Full-System Gap Audit — 2026-11-19

**Scope:** current `main` of `sethfrnz33-droid/BrickOS` as examined on 2026-11-19, including current `firmware/esp32/main`, browser app, the INI parser, Seth's catalogue and reasoner, `prototype/engine-profile-policy.mjs`, `design/` contracts, HP Academy comparison, and the 3DS/DSi project headers. This is **source review**, not an ESP-IDF cross-build or physical ECU verification. `main` changes over time; use a source commit hash from Git history before reproducing the audit.

**Product scope:** an optional, independent Guided Tuning mode that combines the HP Academy *public* ten-stage progression, Seth's personality and diagnostic case manager, an **engine-specific archived hardware profile**, dynamic interpretation of a verified matching rusEFI INI, read-only analysis and eventual separately validated/supervised bounded fuel calibration actions. Phone/browser first; DS/DSi connections and Manual Tuning unchanged. GDI and mixed GDI/PFI **explicitly unsupported** by BrickOS Guided Fuel Tuning.

## Bottom line

The product *architecture* has extensive documentation and a credible division of responsibilities, but the working software still has independent pieces rather than an integrated guided autotune agent. The most consequential missing work is **truthful release/capability inventory, persistent engine-specific data, verified INI-to-feature mapping, high-rate ESP evidence capture, a real diagnostic task coordinator, and tightly separated authorized calibration control**. The story dialogue currently has more behavior than the underlying engine automation can support; do not display a polished narrated experience as if a physical test was verified.

## Verified source-state snapshot

| Component | Current reality as observed from source | Not established |
| --- | --- | --- |
| ESP runtime | `/api/status` reports `brick_os_build=v4.8`; native USB bridge, /api/ts, /api/live, INI cache, logging, browser asset serving | A production `/api/autotune/*` or `/api/guided/v1/*` runtime on current `main` |
| Telemetry | `telemetry.c`: one shared snapshot producer, requests 10/20/50/100 Hz; browser interest expires after ~4 s; multi-1024-byte reads | Guaranteed real 100 Hz, synchronized engine-edge sampling, dedicated pre/post event recorder |
| Existing generic logger | `/api/log/start` and `/api/log/auto` have bounded intervals; independent of guide case archive | Agent-armed pre-event ring, scenario-triggered captures, source timing/quality reconciliation |
| Phone guide | `guide-catalog.json` plus `guide-story.mjs` produce a 10-step HPA-style story, 30 chapter questions and static topic branches | Natural-language interpreter and scenario case actions wired into the browser |
| Phone reasoning | `guide-reasoning.mjs` runs deterministic heuristics; `guided.mjs` stores at most 160 history points over 30 s with >=100 ms spacing | Independent multi-event diagnosis, external truth, full state-machine classification |
| Phone VE evidence | Simplified **8×8 RPM/MAP buckets**, live lambda/target/trim observations, browser-owned read-only learning | Actual active VE lookup interpolation, strategy-aware bank/secondary maps, RAM-only changes |
| Engine archive | `design/engine-build/profile-contract.v1.json` + pure `prototype/engine-profile-policy.mjs` | ESP persistence, phone profile intake, authenticated archive edits, profile/ECU reconciliation UI |
| GDI policy | Explicitly blocked inside **offline profile policy prototype** | Enforced at runtime in all future fuel expert and control endpoints |
| Manual tuning | Existing /api/ts editor path and independent navigation, separate from Guide | Tests proving new coordinator doesn't change semantics for simultaneous 3DS/TunerStudio clients |
| Console support | 3DS/DSi sources and display plans present | Portable guide case packet **consumer** integrated on consoles |
| Verification | Host JS tests and CI exist, including archive and scenario schema checks | Full hardware bench timing/USB-stress and supervised ECU results |

**Stale documentation discovered:**
- `docs/00-current-status.md` describes a **previous v4.12 project**, including v4.9 autotune, v4.11 guide and v4.12 motion paths, not this repository's imported current ESP runtime.
- `docs/11-api-and-state.md` labels `/api/autotune/*`, `/api/tune/guide`, `/api/fueltrims/*` and `/api/tune/motion` as routes observed in that **legacy prototype**, not routes deployed on current `main`. Do not let clients assume they exist.
- Root README still says 3DS v6.3 and DSi v2.2; `apps/3ds/README.md` identifies current source as **v6.5**, and `apps/dsi/README.md` identifies **v29**. Also, the 3DS README itself retains an old v3.7 lead paragraph after its v6.5 announcement.
- Documented API namespaces vary among `/api/autotune`, `/api/guided/v1` and `/api/guide/v1`. Production needs a single registry and versioned capability negotiation.

## P0 — important missing system contracts or release blockers

| ID | Concrete gap | Evidence/source | Required decision / acceptance |
| --- | --- | --- | --- |
| P0-01 | **Release capability matrix is wrong/inconsistent** | `docs/00`, `docs/11`, root and app READMEs | One machine-readable feature manifest per built firmware: `proposed / code exists / host-tested / hardware-tested / ECU-version-verified`; static versioned docs must not override runtime. |
| P0-02 | **No integrated guide coordinator** | Dialogue and analyzer are separate, only phone-local | Define one task FSM: `not applicable / awaiting profile / awaiting ECU / awaiting evidence / awaiting qualified check / analyzed / user-reviewed / externally validated / stale` with task ownership, dependency, audit and return-to-chapter context. |
| P0-03 | **Engine profile is not in live product** | `prototype/engine-profile-policy.mjs` only | Implement opt-in, ESP-owned *engine-specific* persistent archive and phone intake; revision history, conflict resolution, export/import and safe recovery after reboot, distinct from ECU definition. No build fact silently upgrades to physically verified. |
| P0-04 | **Hard GDI exclusion is not enforced in the actual runtime guide** | Only offline policy knows PFI/GDI/mixed | All fuel experts and future write endpoints enforce `PFI / GDI / mixed / unknown / conflicting`; GDI/mixed cannot access fuel auto operations or a PFI fallback, even if firmware INI exposes fields. General read-only diagnostics remain available. |
| P0-05 | **INI semantic coverage does not identify active algorithm reliably** | `core.mjs` decodes some fields/menu metadata; `guided.mjs` resolves name aliases | Validate signature/layout/conditional expressions, actual enabled strategy, target tables and interpolation, active fuel mode, lambda bank selection, scaling and correction semantics. Emit `unsupported` not guesses. Add verified fixtures for several firmware/INI variants. |
| P0-06 | **High-rate evidence exists only as a design** | Shared live snapshot, no case ring; browser history decimated to ~10 Hz | Implement ESP-owned bounded pre/post-event ring, actual delivered-rate metrics, timestamp uncertainty, trigger-specific capture only if firmware supports it, buffer/storage failure tests, no blocking of existing USB activity. |
| P0-07 | **No independent multi-event diagnostic case engine** | Current browser rule heuristics use short single-window data | Separate observation/pattern/hypothesis/contradiction/missing-evidence; compare repeated *independent* events by active engine configuration, provenance and environmental conditions; missing input -> `unknown`, never a confident culprit. |
| P0-08 | **Not a VE autotuner yet** | `guided.mjs` collects 8×8 MAP/RPM evidence buckets and does not write | Build reviewed **PFI-specific** estimator using the active ECU lookup axes, time-aligned lambda, sensor validity, trims/banks, exclusion state, interpolation, repeatable coverage, cell locks, correction limits and source-revision audit. Do not call generic buckets an actual VE map. |
| P0-09 | **No secure calibration authority model** | Wi-Fi AP is open; `/api/ts` uses an arming time window and request signatures, not authenticated paired client identity | Security/threat-model audit for any future agent control: paired operator, replay resistance, exclusive writer and last-acknowledged ECU transaction. A heartbeat, IP or static confirmation phrase is not authentication; never link read-only case endpoints to manual write privileges. |
| P0-10 | **No verified controlled-apply/rollback lifecycle** | RAM proposal, acknowledgement, readback and save are documents only | Before considering implementation: matched revision snapshot, conflict detection, bounded operation scope, device loss policy, explicit separate burn, error statuses for indeterminate partial writes, no blind rollback during unstable conditions, hardware test gates. |
| P0-11 | **Agent questions aren't actionable evidence tasks** | Chapter questions store `reported/unknown`; labels do not integrate ownership or physical proof | Produce `TaskPlan` with exact supported observation, optional supervised external check, reason, expected evidence, unknowns and manual target; user report does not mark physical success. |
| P0-12 | **No credible end-to-end release validation** | Most tests are host/browser; no production ESP-IDF cross-build or ECU validation established | Test full path on ESP32-S3, target rusEFI, fuel-irrelevant negative cases, concurrent clients, USB error injections, resource budgets, roundtrip calibration snapshots, and field-evidence provenance. No live-apply feature shipped solely on host tests. |

## P1 — missing depth and edge cases (material to user experience)

| ID | Gap | Required work |
| --- | --- | --- |
| P1-01 | **Physical build profile not comprehensive** | Add conditional fields for trigger wheel/sensor, cam phase, coils and drivers, injector operating strategy/characterization, actuator types, throttle/pedal, sensor wiring, wideband(s), MAP/MAF/Alpha-N, idle mechanism, VVT, boost, flex/ethanol, CAN/other external modules, ECU hardware variant and pin mapping; ask only when relevant, not via giant mandatory form. |
| P1-02 | **Pump-control versus pressure model not fully resolved** | Keep relay/PWM/OEM controller separate from mechanical/electronic/hybrid regulation, rail sensor presence and absolute/gauge/differential measurement; no regulator verified? Still pressure-model unknown. Link to injector pressure reference and characterization, not merely pump duty. |
| P1-03 | **Cold/hot start and after-start event diary missing** | Build per-event states and quality: sensor not ready, supply/trigger issues, after-start, warmup temperature buckets, comparable starts across revisions. Ordinary lambda can be invalid during crank. |
| P1-04 | **Idle tuning expert not full decision model** | Recognize active idle mode, target, actuator reserve, command vs RPM temporal order, spark corrections, fuel trims, electrical/mechanical load, temperature and idle entry/exit; avoid PID-only diagnoses. |
| P1-05 | **Sensor commissioning too generic** | Sensor calibration, native units, supply/reference limits, analog versus digital health, sensor response, MAP/TPS/pedal plausibility, wideband readiness, fuel/oil pressure, fault channels and external verification evidence. |
| P1-06 | **Trim-aware bank-specific fueling incomplete** | Validate rusEFI `Gego` vs per-bank STFT/LTFT scaling, target-lambda active source, closed-loop masking, trim-learning flags and LTFT availability; no assumption of one bank or universal stoichiometric idle/cruise. |
| P1-07 | **Transient and sensor-delay attribution incomplete** | Differentiate acceleration enrichment, DFCO, recovery, after-start, WUE and active closed-loop corrections; assign each to the owning expert, not VE. Estimate delays only from qualified data; do not invent instantaneous lambda response. |
| P1-08 | **Menu deep links are fuzzy** | `findManualTarget` chooses first regex-matching menu entry. Instead resolve canonical INI page/item/field IDs including conditions, visibility, parent panels and nested folder links, with an explicit unmatched state. |
| P1-09 | **User's free-text questions aren't understood by a real intent parser** | Versioned intent grammar/synonyms, negation/operating-state parsing, ambiguous-input clarification, typed statement provenance. The existing 57-scenario JSON is **design only** and not wired to live case creation. |
| P1-10 | **Reasoning isn't yet a causal evidence model** | Separate causes and responses, rate/timing uncertainty, multiple plausible simultaneous faults, repeated-visit independence, safe next-question selection, counterevidence, confidence calibration against known simulated faults, and regression detection. |
| P1-11 | **No stable multi-client session model** | One active authenticated controller, multiple read-only viewers (3DS/phone), device/ECU reconnect semantics and no UI-rate coupling, conflict with TunerStudio/manual edits, independent identity/profile selection. |
| P1-12 | **No persistent cases and comparable historical baselines** | Store immutable event evidence + user annotations + ECU revision + engine-profile revision, privacy opt-in, export/replay and firmware migration. New calibration epochs invalidate only relevant conclusions. |
| P1-13 | **Full Tour completion logic is shallow** | Distinguish a page read, reported check, ECU observation, qualified external verification and tune-validation scope, and mark incomplete/stale from dependency changes; no false `100% tuned` status. |
| P1-14 | **Phone agent UI beyond story isn't built** | Profile onboarding, case creation, state assessment, capture button, before/after timeline, question explanation, comparison and review workflow; preserve existing Manual Tune. |
| P1-15 | **3DS portability is a proposal rather than a tested consumer** | Stable versioned portable task/case packets, pagination, reduced-size plots, text/voice localization fallback, packet/data ownership; no recompile per INI. Defer DS source changes until phone backend contract stabilizes. |
| P1-16 | **Safety-aware system assessment gaps** | Report ECU fault/protection bits, relevant pressure and temperature warnings, knock as warning, sensor health, context and independent verification; per-subsystem `consistent / unknown / review` rather than global `optimal` or `safe`. |
| P1-17 | **Timing/Smart Spark lacks reliable torque reference** | No numeric advance from MAP/airflow/knock absence or uncalibrated IMU. Make qualified instrumented torque comparisons and actual active corrections distinct; preserve a read-only missing-evidence conclusion. |
| P1-18 | **Storage/reliability resource budget undefined** | Quantify memory/PSRAM availability, case retention, SD/NAND capacity *when actually present*, USB latency, multi-chunk frame skew, logger write throughput, power-loss recovery, watchdog handling, event loss and desktop/client coexistence. |
| P1-19 | **Controller/user intent not formally pinned to the physical engine** | Multi-engine handling, deliberate profile selection, ECU replacement/moving to another vehicle, stale physical assumptions, profile import conflict and warnings before associating a new ECU. |
| P1-20 | **Calibrations cross-affect downstream experts** | Semantically typed dependency/invalidation graph: injector model or pressure reference changes invalidate fuel estimates; sensor calibration changes invalidate dependent evidence; table axis/fuel mode change invalidates mapping; manual edits win over stale proposals. |

## P2 — high-value features still not scheduled as concrete implementation tickets

- **Show all parameters with meaningful coverage statuses:** the INI Atlas lists decoded fields but does not yet provide a verified owned expert, actual active condition, upstream dependencies, measured relevance and `Unknown semantics` fallback.
- **Cross-compare different build states:** modified sensors, fuel composition changes, changed injector hardware, ECU replacement and firmware/INI upgrade while preserving all original history.
- **Inspector for `why this channel?`:** trace measured actual spark, fuel commands and active corrections back to the exact supported INI source and record gaps in the decomposition instead of inventing relationships.
- **Learn from naturally occurring events:** the same passive case data should serve first-start, fuel trim, idle and sensor experts without requesting repetitive observations; only *eligible* samples count.
- **Task handoff and recovery:** Seth should expose `What I can analyze`, `What the user/qualified technician must verify`, `Why we're blocked`, `What changes this conclusion`, `Show exact original setting`, and `Resume at current chapter`.
- **Explicit no-calibration / no-connection experience:** Seth can teach textbook concepts without claiming live ECU readings; user can import compatible prior logs and revisit findings offline, correctly tagged as archived.
- **Localization and accessibility:** text is canonical, mascot voice/jaw optional, sound muted on faults, legible phone/3DS layouts, source/age labels, and conversation content versioning across clients.
- **Manufacturing/pinout-specific commissioning:** physical Brick ECU capability and output assignment mapping should be bound to the exact hardware revision and its documented electrical limits. The INI may declare a logical output even when a customer has not physically connected or validated it.
- **Automated regression/fuzz suite:** arbitrary INI sizes/types/conditions, duplicate menu labels, hostile strings, engine-type conflicts, sensor/channel loss, repeatability, memory exhaustion, and simultaneous editor/guide operations.

## Highest-value concrete missing test scenarios

1. **Engine PFI + PWM pump + unknown regulator:** Seth asks pressure regulation without pretending it is unregulated, never calls rail pressure known, and still offers non-fuel explanatory pages.
2. **PWM with physical mechanical regulator:** retains both pump and regulation branches; no false claim electronic feedback exists.
3. **No rail pressure sensor:** cannot infer pressure from duty; does not show 0 kPa or claim verified injector differential pressure.
4. **GDI/mixed hardware paired with an INI exposing some PFI fields:** hard runtime block on PFI fuel expert; no automatic proposal or GDI setup instructions.
5. **ECU software says PFI but archived build says GDI:** show explicit contradiction; profile not silently overwritten.
6. **Different engine on same ECU:** require deliberate profile selection before using old hardware facts.
7. **Valid lambda equals target with non-neutral STFT/LTFT:** note trim masking and leave base VE unverified.
8. **Lambda sensor not ready at an observed cold start:** no fueling fault inferred from missing lambda.
9. **Idle RPM varies while actuator response lags:** rank competing load/feedback/fuel explanations, not a single PID gain diagnosis.
10. **Only trigger error count available:** do not invent missing-tooth timing; offer ECU-specific special trace *if supported*.
11. **Alpha-N or alternate load strategy:** evidence map uses the active axes, never quietly MAP bins.
12. **Two banks or second VE strategy:** sources/trim lambda attribution remains bank- and table-specific.
13. **Config revision changes during guided session:** invalidate affected evidence/proposals; no replay of previous edit.
14. **Manual editor, 3DS TCP bridge and phone case capture concurrently:** prioritize ECU communications; quantify delivered sample rate/overruns, avoid lost acknowledgements.
15. **Phone disconnect or ESP reboot:** active learning session revoked; recording/recovery state reported, no surprise write or silent resume.
16. **Unknown sensor/channel or malformed INI:** show `unsupported` and preserve matching raw evidence only; no inferred zero or made-up recommendation.
17. **Cold/warm event comparisons with different ambient/engine state:** refuse direct causal claims and display missing comparability.
18. **Qualified dyno evidence unavailable:** Smart Spark says validation unconfirmed, not MBT reached.

## Recommended implementation order, dependency-driven

**M0 — Fix source truth:** correct current/legacy version docs, one runtime capabilities manifest and authoritative namespace. Include provenance labels in Guide. No ECU command changes.

**M1 — Live engine archive and onboarding:** use tested pure profile policy as starting point; add conflict-aware versioned persistence and phone UI; verify profile can survive INI changes, be exported, and cannot silently assert physical checks. Enforce GDI gate in the same future centralized expert registry.

**M2 — Real INI semantic resolver:** active strategy/conditions, menu/field identities, sensors/units/banks, correction semantics, unsupported fallbacks. Build replay fixtures and cross-version contract tests before more experts.

**M3 — ESP evidence service and case recorder:** timestamped non-blocking acquisition, bounded ring, event storage/trace, actual performance limits and robust loss reporting. Phone should only subscribe to summaries.

**M4 — Guided coordinator and intelligent diagnostics:** case FSM, questions, profile context, scenario registry integration, multiple hypotheses, source/revision audit, and HPA task handoffs. Keep read-only.

**M5 — Fuel learning proposals:** verified PFI-only strategy-aware estimator, active table/interpolation, robust sample eligibility and independent event evidence. Preview reviewable diffs only, separate from the manual editor.

**M6 — Phone full experience and source-independent packets:** Seth's conversational experience, typed problem input, engine archive, before/during/after evidence, resume, exports, full chapter/task navigation and manual deep links.

**M7 — Controlled RAM edits (separate security and validation project):** only after independent threat modeling, verified ECU-specific memory semantics, supervision and bench validation. There must be a separate decision whether to ship this at all. GDI/mixed remain unsupported.

**M8 — Later 3DS port:** consume the proven ESP-owned packet contract without changing existing manual connection protocols or hardcoding dialogue, semantic rules, INI offsets or engine profiles in the console.

## What this review does *not* claim

- Host tests cannot demonstrate an ECU is configured correctly or running safely.
- A 100 Hz *request* does not imply 100 Hz delivered, tooth-level trigger resolution or exactly synchronized channels.
- A user-entered hardware description does not independently verify physical fuel pressure, sync reference or wiring.
- Stable lambda, controlled idle and absence of reported knock do not establish universally optimal calibration.
- No architecture document here enables real-time engine control, automatic physical tests or calibration changes on the current ECU.

## External source cross-checks

- HPA public curriculum: https://www.hpacademy.com/courses/practical-standalone-tuning/getting-started-the-hpa-10-step-process/ (the conceptual ten-stage order).
- rusEFI fuel compensation and pressure-reference fields: https://wiki.rusefi.com/Fuel-System/ ; https://wiki.rusefi.com/Fuel-Pressure/ (PWM control does not establish a regulator, pressure feedback or injector differential pressure).
- rusEFI logging: https://wiki.rusefi.com/Logging-Guide/ (different logging sources/cadences and necessary source context).
- rusEFI generated INI and conditional capabilities vary by firmware; definitive mapping must be done from a matching ECU/INI snapshot, not an unverified generic lookup.

**Status note:** this audit is a maintained engineering backlog, not an implementation announcement. Update checkboxes/status only against concrete commits and hardware evidence.
