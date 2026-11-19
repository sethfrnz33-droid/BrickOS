# Seth — Complete Guided Tuning Dialogue Script

**Source of truth:** `firmware/esp32/web/guide-catalog.json` (version 1.0.0). This document is generated from that same catalogue; edit the JSON and regenerate this file to keep phone and future DS dialogue identical.

**Character / lore:** Seth is a deliberately low-key clone of the Brick ECU creator. He is literally stuck inside the ECU and mildly fed up with the arrangement, but wants the user to succeed. His dry asides may make the process memorable, but faults, safety constraints and exact observations are always presented clearly. The visual portrait is from the user-provided pixel art; a split jaw animates alongside the user-provided gibberish recording. His voice is nonverbal; the typed on-screen dialogue conveys all meaning.

**Conversation modes:** Full tour, targeted assistance (nine subject gateways), and INI Atlas field-by-field walkthrough. This document includes **all 88 fixed dialogue nodes**; field-by-field descriptions are generated at runtime from the actual matched INI and therefore cannot be exhaustively prewritten.

**Markers:** `{{available:a|b}}` is replaced by the role-bound ECU observations if present and fresh, or an explicit unknown/missing message. A `go` target beginning `workspace.` changes the browser view, `action.manual.` opens only a verified existing menu, and `field.N` is an automatically generated INI field help page.

## `welcome` — general

**Seth:** Hey. I'm Seth. Well, an ECU-bound copy of Seth. Long story. I live in here now, and apparently helping you is my entire job. So—where do we start?

**Choices:**
- Take me through the whole ECU → `tour.start`
- I need help with something → `targeted`
- Continue where I left off → `resume`
- Walk through INI settings → `atlas.entry`
- See ECU evidence / full INI atlas → `workspace`

## `resume` — general

**Seth:** I can pick up where we left off, but first I need to confirm we're still looking at the same ECU definition. I'm not trusting yesterday's offsets on today's firmware.

**Choices:**
- Resume my last topic → `resume.last`
- Start at the beginning → `tour.start`
- Choose another topic → `targeted`

## `targeted` — general

**Seth:** All right. What are we dealing with? Pick the closest topic; we can narrow it down. I'm not going to make you read a 900-page INI unless you're into that.

**Choices:**
- Starting & warmup → `topics.start`
- Idle → `topics.idle`
- Fuel & AFR → `topics.fuel`
- Sensors & wiring → `topics.sensors`
- Trigger & synchronization → `topics.trigger`
- Ignition & knock → `topics.spark`
- Connection & INI → `topics.connection`
- More systems → `topics.more`
- Not sure: help me find it → `topics.unknown`

## `tour.start` — general

**Seth:** Full tour it is. I'll follow the systems your ECU actually exposes, keep track of what we have verified, and flag what needs someone to check in the real world. None of this changes your tune by itself.

**Choices:**
- Check ECU identity and INI → `tour.identity`
- See the complete tour map → `tour.map`
- Change to targeted help → `targeted`

## `tour.map` — general

**Seth:** The route is: ECU identity, sensors, synchronization, starting, idle, warmup, fuel model, fuel corrections, drivability, ignition review, optional systems, and final validation. We'll skip what's genuinely not applicable—not what's merely missing data.

**Choices:**
- Begin with ECU identity → `tour.identity`
- Walk through INI settings → `atlas.entry`
- Choose a system instead → `targeted`

## `tour.identity` — ECU identity

**Seth:** First we confirm the ECU identity and exact matching INI. If the definition is wrong, the numbers may point to the wrong memory. I am not guessing those.

**Choices:**
- What does the ECU show? → `evidence.identity`
- Find related settings in the INI → `manual.identity`
- Continue: Sensors → `tour.sensors`
- Choose another topic → `targeted`

## `tour.sensors` — Sensors

**Seth:** Next, the sensors. We check which live channels exist, which readings are fresh, and which still need physical verification. A number on the screen does not prove the wiring is correct.

**Choices:**
- What does the ECU show? → `evidence.sensors`
- Find related settings in the INI → `manual.sensors`
- Continue: Trigger and synchronization → `tour.trigger`
- Choose another topic → `targeted`

## `tour.trigger` — Trigger and synchronization

**Seth:** The ECU needs a trustworthy crank/cam reference. I can read reported sync-related evidence where the firmware exposes it; physical timing reference is a separate check.

**Choices:**
- What does the ECU show? → `evidence.trigger`
- Find related settings in the INI → `manual.trigger`
- Continue: First-start observations → `tour.starting`
- Choose another topic → `targeted`

## `tour.starting` — First-start observations

**Seth:** Here we separate cranking, catching, after-start, and stable running. Different systems own different parts of that timeline, so we do not treat every start problem as a VE error.

**Choices:**
- What does the ECU show? → `evidence.starting`
- Find related settings in the INI → `manual.starting`
- Continue: Idle behavior → `tour.idle`
- Choose another topic → `targeted`

## `tour.idle` — Idle behavior

**Seth:** Idle is air, fuel, spark, hardware and feedback working together. We identify which strategy is enabled before blaming a PID value or editing a table.

**Choices:**
- What does the ECU show? → `evidence.idle`
- Find related settings in the INI → `manual.idle`
- Continue: Warmup → `tour.warmup`
- Choose another topic → `targeted`

## `tour.warmup` — Warmup

**Seth:** A warm idle only tells us about warm idle. We need independent temperature-specific observations for cold-start and enrichment behavior.

**Choices:**
- What does the ECU show? → `evidence.warmup`
- Find related settings in the INI → `manual.warmup`
- Continue: Base fueling and VE → `tour.fuel`
- Choose another topic → `targeted`

## `tour.fuel` — Base fueling and VE

**Seth:** We compare valid lambda evidence with target and the active fuel strategy. Closed-loop trims and transient enrichment can hide a bad base table, so those cases are not blindly counted.

**Choices:**
- What does the ECU show? → `evidence.fuel`
- Find related settings in the INI → `manual.fuel`
- Continue: Fuel trims → `tour.trims`
- Choose another topic → `targeted`

## `tour.trims` — Fuel trims

**Seth:** Short-term and long-term trim are different signals with firmware-dependent scaling. I show each available bank separately and say when correction semantics are unknown.

**Choices:**
- What does the ECU show? → `evidence.trims`
- Find related settings in the INI → `manual.trims`
- Continue: Throttle transitions → `tour.transients`
- Choose another topic → `targeted`

## `tour.transients` — Throttle transitions

**Seth:** Steady fueling and sudden throttle changes are different jobs. We do not teach VE from enrichment, fuel cut, or recovery events until they can be classified.

**Choices:**
- What does the ECU show? → `evidence.transients`
- Find related settings in the INI → `manual.transients`
- Continue: Ignition evidence → `tour.spark`
- Choose another topic → `targeted`

## `tour.spark` — Ignition evidence

**Seth:** Ignition timing affects torque and knock margin, but MAP, AFR and acceleration alone cannot establish best-torque timing. We can organize evidence and flag the need for instrumented verification.

**Choices:**
- What does the ECU show? → `evidence.spark`
- Find related settings in the INI → `manual.spark`
- Continue: Optional ECU systems → `tour.advanced`
- Choose another topic → `targeted`

## `tour.advanced` — Optional ECU systems

**Seth:** Electronic throttle, boost, VVT, flex fuel, CAN and other extras only appear when the INI and current calibration show they apply. Missing support is shown, not made up.

**Choices:**
- What does the ECU show? → `evidence.advanced`
- Find related settings in the INI → `manual.advanced`
- Continue: Final review → `tour.validation`
- Choose another topic → `targeted`

## `tour.validation` — Final review

**Seth:** Now we audit what was really measured, what was only inferred, which settings changed, and what still requires qualified verification. I am not putting a fake 100%-tuned badge on this engine.

**Choices:**
- What does the ECU show? → `evidence.validation`
- Find related settings in the INI → `manual.validation`
- Continue: Finish tour → `tour.done`
- Choose another topic → `targeted`

## `tour.done` — general

**Seth:** That's the tour of the systems we've identified. A completed tour means we've accounted for the configuration—not that the engine has been mechanically verified or that every map is optimized. Want to look at a specific finding?

**Choices:**
- Open evidence and INI atlas → `workspace`
- Get help with a symptom → `targeted`
- Walk through INI settings → `atlas.entry`
- Return to guide home → `welcome`

## `topics.start` — Starting & warmup

**Seth:** Is it refusing to start, catching and then quitting, or just behaving differently hot and cold?

**Choices:**
- Cranks but does not catch → `issue.start.no_catch`
- Catches, then stops → `issue.start.catch_quit`
- Cold-start behavior → `issue.start.cold`
- Hot-start behavior → `issue.start.hot`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

## `issue.start.no_catch` — Cranks but does not catch

**Seth:** A crank-no-catch situation has several possible causes: synchronization, sensor validity, ignition reference or fueling. We should read the ECU fault and sync evidence before deciding a fuel table is wrong. What I can read right now: {{available:sync|rpm|clt}}.

**Choices:**
- Show relevant ECU readings → `evidence.trigger`
- Open the original ECU setting → `manual.trigger`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.start.catch_quit` — Catches, then stops

**Seth:** That transition can involve after-start fueling, idle airflow or other protections. We need a start-event timeline and verified sensors before assigning blame. What I can read right now: {{available:rpm|clt|idleCommand}}.

**Choices:**
- Show relevant ECU readings → `evidence.starting`
- Open the original ECU setting → `manual.starting`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.start.cold` — Cold-start behavior

**Seth:** Cold and warm starts are separate observations. We should compare start events by initial coolant temperature and wideband readiness; one event is not enough to tune a curve. What I can read right now: {{available:clt|rpm}}.

**Choices:**
- Show relevant ECU readings → `evidence.warmup`
- Open the original ECU setting → `manual.warmup`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.start.hot` — Hot-start behavior

**Seth:** Heat-soak and warm-start conditions need their own evidence. We should check sensor plausibility and start history before assuming the cranking table is wrong. What I can read right now: {{available:clt|rpm}}.

**Choices:**
- Show relevant ECU readings → `evidence.starting`
- Open the original ECU setting → `manual.starting`
- Back to topics → `targeted`
- Guide home → `welcome`

## `topics.idle` — Idle

**Seth:** What part of idle is bothering you?

**Choices:**
- RPM surges or oscillates → `issue.idle.surge`
- Idle is too low or stalls → `issue.idle.low`
- Idle is too high → `issue.idle.high`
- Idle changes when loads appear → `issue.idle.load`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

## `issue.idle.surge` — RPM surges or oscillates

**Seth:** The big question is whether the idle actuator moves before the RPM does or follows it. Timing between signals helps separate unstable feedback from external disturbances. What I can read right now: {{available:rpm|idleCommand|idleTarget}}.

**Choices:**
- Show relevant ECU readings → `evidence.idle`
- Open the original ECU setting → `manual.idle`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.idle.low` — Idle is too low or stalls

**Seth:** A low idle can involve insufficient airflow authority, load, incorrect sensor inputs or combustion behavior. Actuator saturation is evidence, not proof that a PID gain is bad. What I can read right now: {{available:rpm|idleCommand|idleTarget}}.

**Choices:**
- Show relevant ECU readings → `evidence.idle`
- Open the original ECU setting → `manual.idle`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.idle.high` — Idle is too high

**Seth:** We should confirm whether the ECU is actually in idle mode and compare target RPM, commanded airflow and throttle feedback before touching the idle calibration. What I can read right now: {{available:rpm|idleCommand|idleTarget|tps}}.

**Choices:**
- Show relevant ECU readings → `evidence.idle`
- Open the original ECU setting → `manual.idle`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.idle.load` — Idle changes when loads appear

**Seth:** Load compensation might be involved, but we need load state, target RPM, actuator and ignition evidence together to explain why it changed. What I can read right now: {{available:rpm|idleTarget|idleCommand|spark}}.

**Choices:**
- Show relevant ECU readings → `evidence.idle`
- Open the original ECU setting → `manual.idle`
- Back to topics → `targeted`
- Guide home → `welcome`

## `topics.fuel` — Fuel & AFR

**Seth:** What part of fueling do you want to understand?

**Choices:**
- Measured lambda differs from target → `issue.fuel.lambda`
- Fuel trims keep correcting → `issue.fuel.trims`
- What does this VE table mean? → `issue.fuel.ve`
- Hesitation when throttle moves → `issue.fuel.throttle`
- Fueling changes with temperature → `issue.fuel.warm`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

## `issue.fuel.lambda` — Measured lambda differs from target

**Seth:** First I check whether the wideband is ready, whether the readings are current, and whether trim or enrichment is active. A mismatch during a transient is not a simple VE-table error. What I can read right now: {{available:lambda|target|lambdaGood|gego}}.

**Choices:**
- Show relevant ECU readings → `evidence.fuel`
- Open the original ECU setting → `manual.fuel`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.fuel.trims` — Fuel trims keep correcting

**Seth:** That could mean closed-loop compensation is covering an underlying error. I keep STFT, LTFT and Gego separate and will not assume they use the same neutral value. What I can read right now: {{available:gego|stft1|ltft1}}.

**Choices:**
- Show relevant ECU readings → `evidence.trims`
- Open the original ECU setting → `manual.trims`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.fuel.ve` — What does this VE table mean?

**Seth:** Think of the VE table as a model indexed by the active RPM and load strategy. The INI tells us the actual shape; our evidence map is not a substitute for the original table editor. What I can read right now: {{available:rpm|map|lambda|target}}.

**Choices:**
- Show relevant ECU readings → `evidence.fuel`
- Open the original ECU setting → `manual.fuel`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.fuel.throttle` — Hesitation when throttle moves

**Seth:** Throttle changes can involve transient fueling and sensor delay. We need to separate those events from stable fueling; otherwise the VE learner will chase the wrong thing. What I can read right now: {{available:tps|map|lambda}}.

**Choices:**
- Show relevant ECU readings → `evidence.transients`
- Open the original ECU setting → `manual.transients`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.fuel.warm` — Fueling changes with temperature

**Seth:** Warmup fuel and steady-state fuel are not the same calibration. We should identify which temperature-based corrections are active before interpreting the lambda change. What I can read right now: {{available:clt|lambda|target}}.

**Choices:**
- Show relevant ECU readings → `evidence.warmup`
- Open the original ECU setting → `manual.warmup`
- Back to topics → `targeted`
- Guide home → `welcome`

## `topics.sensors` — Sensors & wiring

**Seth:** Which signal looks suspicious?

**Choices:**
- Wideband or lambda → `issue.sensors.lambda`
- MAP or TPS → `issue.sensors.load`
- Coolant or intake temperature → `issue.sensors.temp`
- Fuel or oil pressure → `issue.sensors.pressure`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

## `issue.sensors.lambda` — Wideband or lambda

**Seth:** We should check whether the ECU declares the lambda channel valid, the value is fresh and the target comes from the right strategy. A plausible number alone does not guarantee sensor health. What I can read right now: {{available:lambda|lambdaGood|target}}.

**Choices:**
- Show relevant ECU readings → `evidence.sensors`
- Open the original ECU setting → `manual.sensors`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.sensors.load` — MAP or TPS

**Seth:** We should compare MAP, TPS, RPM, and their configured input modes. Incorrect sensor scaling can look exactly like an incorrect fuel model. What I can read right now: {{available:map|tps|rpm}}.

**Choices:**
- Show relevant ECU readings → `evidence.sensors`
- Open the original ECU setting → `manual.sensors`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.sensors.temp` — Coolant or intake temperature

**Seth:** Temperature readings influence starting, warmup, idle and protection logic. If the measurement is uncertain, dependent tuning results have to be reviewed. What I can read right now: {{available:clt}}.

**Choices:**
- Show relevant ECU readings → `evidence.sensors`
- Open the original ECU setting → `manual.sensors`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.sensors.pressure` — Fuel or oil pressure

**Seth:** Pressure monitoring is important, but the available channel must be mapped to an actually installed and calibrated sensor. Missing hardware is not a zero reading. What I can read right now: {{available:fuelPressure|oilPressure}}.

**Choices:**
- Show relevant ECU readings → `evidence.sensors`
- Open the original ECU setting → `manual.sensors`
- Back to topics → `targeted`
- Guide home → `welcome`

## `topics.trigger` — Trigger & sync

**Seth:** What does the ECU report or what needs verification?

**Choices:**
- Synchronization errors → `issue.trigger.no_sync`
- Ignition reference unclear → `issue.trigger.reference`
- Intermittent sync loss → `issue.trigger.intermittent`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

## `issue.trigger.no_sync` — Synchronization errors

**Seth:** We first check whether the ECU exposes sync status or error counters and whether the event is current. Physical wiring and trigger shape still require external verification. What I can read right now: {{available:sync|rpm}}.

**Choices:**
- Show relevant ECU readings → `evidence.trigger`
- Open the original ECU setting → `manual.trigger`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.trigger.reference` — Ignition reference unclear

**Seth:** The commanded spark number is not proof of actual crankshaft timing. A qualified physical reference verification is required before treating ignition calculations as trustworthy. What I can read right now: {{available:spark|sync}}.

**Choices:**
- Show relevant ECU readings → `evidence.trigger`
- Open the original ECU setting → `manual.trigger`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.trigger.intermittent` — Intermittent sync loss

**Seth:** I would compare timestamped faults with RPM, battery and relevant conditions. The guide should not blame a tooth setting solely from one error count. What I can read right now: {{available:sync|rpm|battery}}.

**Choices:**
- Show relevant ECU readings → `evidence.trigger`
- Open the original ECU setting → `manual.trigger`
- Back to topics → `targeted`
- Guide home → `welcome`

## `topics.spark` — Ignition & knock

**Seth:** What are you trying to figure out about ignition?

**Choices:**
- Where should ignition timing be? → `issue.spark.timing`
- Knock or timing corrections → `issue.spark.knock`
- Ignition behavior at idle → `issue.spark.idle`
- Validate timing on a dyno → `issue.spark.verify`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

## `issue.spark.timing` — Where should ignition timing be?

**Seth:** Engine geometry and fuel characteristics help describe the problem, but they do not calculate a safe best-torque map. We can review the active spark strategy and what evidence is missing for validation. What I can read right now: {{available:spark|knock}}.

**Choices:**
- Show relevant ECU readings → `evidence.spark`
- Open the original ECU setting → `manual.spark`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.spark.knock` — Knock or timing corrections

**Seth:** A reported knock event or retard needs source, RPM/load and correction context. The absence of reported knock does not prove timing can be advanced. What I can read right now: {{available:knock|spark|rpm|map}}.

**Choices:**
- Show relevant ECU readings → `evidence.spark`
- Open the original ECU setting → `manual.spark`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.spark.idle` — Ignition behavior at idle

**Seth:** Idle spark may act as a fast stabilizer. We need to distinguish the commanded base advance from active corrections before interpreting RPM variation. What I can read right now: {{available:spark|rpm|idleTarget}}.

**Choices:**
- Show relevant ECU readings → `evidence.idle`
- Open the original ECU setting → `manual.idle`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.spark.verify` — Validate timing on a dyno

**Seth:** Instrumented torque evidence and controlled conditions are needed to make credible torque comparisons. The software can organize and explain that evidence without autonomously advancing timing. What I can read right now: {{available:spark|knock}}.

**Choices:**
- Show relevant ECU readings → `evidence.validation`
- Open the original ECU setting → `manual.validation`
- Back to topics → `targeted`
- Guide home → `welcome`

## `topics.connection` — Connection & INI

**Seth:** What part of the connection is causing trouble?

**Choices:**
- Phone cannot reach BrickOS → `issue.connection.wifi`
- ESP does not see the ECU → `issue.connection.ecu`
- INI will not match → `issue.connection.ini`
- Readings stop updating → `issue.connection.stale`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

## `issue.connection.wifi` — Phone cannot reach BrickOS

**Seth:** Confirm the phone is on the ESP access point and that the browser can reach the device. The open AP is not authentication for automatic calibration writes.

**Choices:**
- Show relevant ECU readings → `evidence.identity`
- Open the original ECU setting → `manual.identity`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.connection.ecu` — ESP does not see the ECU

**Seth:** The browser and ECU transport are separate. A working Wi-Fi page does not prove native USB is enumerated or that the ECU session is healthy.

**Choices:**
- Show relevant ECU readings → `evidence.identity`
- Open the original ECU setting → `manual.identity`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.connection.ini` — INI will not match

**Seth:** The generated INI must match the live ECU firmware signature and layout; a similar name is not sufficient. We should inspect the definition status rather than guessing offsets.

**Choices:**
- Show relevant ECU readings → `evidence.identity`
- Open the original ECU setting → `manual.identity`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.connection.stale` — Readings stop updating

**Seth:** A stale screen must say stale rather than showing a plausible old number. We should inspect current transport status before making any tuning judgment. What I can read right now: {{available:rpm}}.

**Choices:**
- Show relevant ECU readings → `evidence.identity`
- Open the original ECU setting → `manual.identity`
- Back to topics → `targeted`
- Guide home → `welcome`

## `topics.more` — Other systems

**Seth:** Which of the remaining systems do you want to investigate?

**Choices:**
- Electronic throttle or idle actuator → `issue.more.dbw`
- Boost control → `issue.more.boost`
- Cam timing or VVT → `issue.more.vvt`
- CAN and auxiliary features → `issue.more.can`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

## `issue.more.dbw` — Electronic throttle or idle actuator

**Seth:** Electronic throttle is a safety-critical subsystem. The guide can locate its defined settings and diagnostics but cannot infer calibration validity from a single command or automatically perform actuator tests. What I can read right now: {{available:tps|idleCommand}}.

**Choices:**
- Show relevant ECU readings → `evidence.advanced`
- Open the original ECU setting → `manual.advanced`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.more.boost` — Boost control

**Seth:** Boost calibration requires verification of the installed hardware, sensors and protection strategy. Unrecognized configurations should remain read-only. What I can read right now: {{available:map}}.

**Choices:**
- Show relevant ECU readings → `evidence.advanced`
- Open the original ECU setting → `manual.advanced`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.more.vvt` — Cam timing or VVT

**Seth:** Cam position targets and feedback must be correctly identified before analyzing control performance. Unknown mappings remain manual-review items. What I can read right now: {{available:rpm}}.

**Choices:**
- Show relevant ECU readings → `evidence.advanced`
- Open the original ECU setting → `manual.advanced`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.more.can` — CAN and auxiliary features

**Seth:** The active INI tells us which dialogs exist; the guide can point to them and explain their dependencies without inventing unavailable hardware. What I can read right now: {{available:rpm}}.

**Choices:**
- Show relevant ECU readings → `evidence.advanced`
- Open the original ECU setting → `manual.advanced`
- Back to topics → `targeted`
- Guide home → `welcome`

## `topics.unknown` — Help me figure it out

**Seth:** No problem. We can start from what the ECU actually reports. I will highlight stale inputs, current faults, missing calibration evidence and the original settings they relate to.

**Choices:**
- Show me the most important observation → `issue.unknown.overview`
- It has trouble starting → `issue.unknown.start`
- It runs but feels wrong → `issue.unknown.running`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

## `issue.unknown.overview` — Show me the most important observation

**Seth:** I would begin by checking identity and fresh measurements. If those are missing, no engine-tuning diagnosis is reliable.

**Choices:**
- Show relevant ECU readings → `evidence.sensors`
- Open the original ECU setting → `manual.sensors`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.unknown.start` — It has trouble starting

**Seth:** Starting diagnostics distinguish sync, cranking, after-start and idle transitions; we will need a specific event before diagnosing. What I can read right now: {{available:rpm|sync}}.

**Choices:**
- Show relevant ECU readings → `evidence.starting`
- Open the original ECU setting → `manual.starting`
- Back to topics → `targeted`
- Guide home → `welcome`

## `issue.unknown.running` — It runs but feels wrong

**Seth:** First we separate stable fueling, transients, idle and ignition evidence. A general symptom alone cannot identify a bad calibration field. What I can read right now: {{available:rpm|lambda|map}}.

**Choices:**
- Show relevant ECU readings → `evidence.fuel`
- Open the original ECU setting → `manual.fuel`
- Back to topics → `targeted`
- Guide home → `welcome`

## `evidence.identity` — Evidence: ECU identity

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:rpm}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.identity`
- Back to the guide → `back`

## `manual.identity` — Manual setting: ECU identity

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Choices:**
- Open matching setting → `action.manual.identity`
- See available evidence → `evidence.identity`
- Back → `back`

## `evidence.sensors` — Evidence: Sensors

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:clt|tps|map|lambda}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.sensors`
- Back to the guide → `back`

## `manual.sensors` — Manual setting: Sensors

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Choices:**
- Open matching setting → `action.manual.sensors`
- See available evidence → `evidence.sensors`
- Back → `back`

## `evidence.trigger` — Evidence: Trigger and synchronization

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:sync}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.trigger`
- Back to the guide → `back`

## `manual.trigger` — Manual setting: Trigger and synchronization

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Choices:**
- Open matching setting → `action.manual.trigger`
- See available evidence → `evidence.trigger`
- Back → `back`

## `evidence.starting` — Evidence: First-start observations

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:rpm|clt}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.starting`
- Back to the guide → `back`

## `manual.starting` — Manual setting: First-start observations

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Choices:**
- Open matching setting → `action.manual.starting`
- See available evidence → `evidence.starting`
- Back → `back`

## `evidence.idle` — Evidence: Idle behavior

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:rpm|idleTarget|idleCommand}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.idle`
- Back to the guide → `back`

## `manual.idle` — Manual setting: Idle behavior

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Choices:**
- Open matching setting → `action.manual.idle`
- See available evidence → `evidence.idle`
- Back → `back`

## `evidence.warmup` — Evidence: Warmup

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:clt}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.warmup`
- Back to the guide → `back`

## `manual.warmup` — Manual setting: Warmup

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Choices:**
- Open matching setting → `action.manual.warmup`
- See available evidence → `evidence.warmup`
- Back → `back`

## `evidence.fuel` — Evidence: Base fueling and VE

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:lambda|target|gego}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.fuel`
- Back to the guide → `back`

## `manual.fuel` — Manual setting: Base fueling and VE

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Choices:**
- Open matching setting → `action.manual.fuel`
- See available evidence → `evidence.fuel`
- Back → `back`

## `evidence.trims` — Evidence: Fuel trims

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:gego|stft1|ltft1}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.trims`
- Back to the guide → `back`

## `manual.trims` — Manual setting: Fuel trims

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Choices:**
- Open matching setting → `action.manual.trims`
- See available evidence → `evidence.trims`
- Back → `back`

## `evidence.transients` — Evidence: Throttle transitions

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:tps|map}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.transients`
- Back to the guide → `back`

## `manual.transients` — Manual setting: Throttle transitions

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Choices:**
- Open matching setting → `action.manual.transients`
- See available evidence → `evidence.transients`
- Back → `back`

## `evidence.spark` — Evidence: Ignition evidence

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:spark|knock}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.spark`
- Back to the guide → `back`

## `manual.spark` — Manual setting: Ignition evidence

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Choices:**
- Open matching setting → `action.manual.spark`
- See available evidence → `evidence.spark`
- Back → `back`

## `evidence.advanced` — Evidence: Optional ECU systems

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.advanced`
- Back to the guide → `back`

## `manual.advanced` — Manual setting: Optional ECU systems

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Choices:**
- Open matching setting → `action.manual.advanced`
- See available evidence → `evidence.advanced`
- Back → `back`

## `evidence.validation` — Evidence: Final review

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.validation`
- Back to the guide → `back`

## `manual.validation` — Manual setting: Final review

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Choices:**
- Open matching setting → `action.manual.validation`
- See available evidence → `evidence.validation`
- Back → `back`

## `workspace` — Workspace

**Seth:** If you want all the evidence, tables and INI field inventory, the existing analysis workspace is still here. This conversation stays separate from Manual Tuning.

**Choices:**
- Open evidence → `workspace.evidence`
- Browse the INI Atlas → `workspace.atlas`
- View systems → `workspace.systems`
- Walk through INI settings → `atlas.entry`
- Guide home → `welcome`

## `atlas.entry` — general

**Seth:** The Configuration Atlas includes every calibration field the active INI parser can decode. We can inspect them one by one, or search the original INI tree. If I don't know what a setting does, I'll tell you instead of making up an answer.

**Choices:**
- Walk through decoded settings → `field.0`
- Search the full INI Atlas → `workspace.atlas`
- Back to guide home → `welcome`

