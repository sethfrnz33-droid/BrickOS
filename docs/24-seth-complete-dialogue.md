# Seth — Complete Guided Tuning Dialogue Script

**Source of truth:** `firmware/esp32/web/guide-catalog.json` (version **1.2.0**).

**HP Academy-inspired structure:** The ten public curriculum stages determine the tour order; the dialogue and explanations are original BrickOS material, not republished course lessons.

**Character:** Seth is the creator's chip-bound clone. The dry humor is secondary to precise explanations of measurements and uncertainty.

**Portability:** The ESP serves the versioned catalogue. The phone and future DS/DSi render its packets; all ECU-specific roles, values and editor links are resolved against the current matching INI.

**Contents:** 97 fixed dialogue nodes plus dynamically generated INI field pages. Visiting a dialogue step never marks a calibration or hardware subsystem verified.

## Default Full Tour — ten chapters

1. **ECU configuration and testing** — `tour.configuration` (expert `identity`).
2. **Trigger setup** — `tour.trigger` (expert `trigger`).
3. **Base table configuration** — `tour.base_tables` (expert `fuel`).
4. **Base ignition / fuel pressure verification** — `tour.reference` (expert `spark`).
5. **Initial startup** — `tour.starting` (expert `starting`).
6. **Idle tuning** — `tour.idle` (expert `idle`).
7. **Steady-state fuel** — `tour.steady_fuel` (expert `fuel`).
8. **Steady-state ignition** — `tour.steady_spark` (expert `spark`).
9. **Full-power instrumented review** — `tour.full_power` (expert `validation`).
10. **Final confirmation** — `tour.confirm` (expert `validation`).

## Every fixed dialogue node

Placeholders of the form `{{available:role|role}}` resolve to actual reported, current, verified INI channels or an explicit unknown/unavailable label.

### `welcome` — General

**Seth:** Hey. I'm Seth. Well, an ECU-bound copy of Seth. Long story. I live in here now, and apparently helping you is my entire job. So—where do we start?

**Choices:**
- Take me through the whole ECU → `tour.start`
- I need help with something → `targeted`
- Continue where I left off → `resume`
- Walk through INI settings → `atlas.entry`
- See ECU evidence / full INI atlas → `workspace`

### `resume` — General

**Seth:** I can pick up where we left off, but first I need to confirm we're still looking at the same ECU definition. I'm not trusting yesterday's offsets on today's firmware.

**Choices:**
- Resume my last topic → `resume.last`
- Start at the beginning → `tour.start`
- Choose another topic → `targeted`

### `targeted` — General

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

### `tour.start` — General

**Seth:** Full tour. We'll use a ten-stage structure based on HP Academy's public outline, then adapt the details to this ECU's matching INI. I'll explain what's verified, what isn't, and where the real settings live. No automatic changes.

**Choices:**
- Begin Step 1: ECU configuration → `tour.configuration`
- See all ten stages → `tour.map`
- Get help with one problem → `targeted`

### `tour.map` — General

**Seth:** Our route has ten chapters: ECU configuration, trigger, base tables, physical references, startup, idle, steady fuel, steady ignition, instrumented power review and final confirmation. Warmup, trims, transients and advanced systems remain available as focused subtopics. Skipping an explanation never verifies a setting.

**Choices:**
- Begin Step 1 → `tour.configuration`
- Jump to a specific issue → `targeted`
- Browse every INI field → `atlas.entry`

### `tour.identity` — ECU identity

**Seth:** First we confirm the ECU identity and exact matching INI. If the definition is wrong, the numbers may point to the wrong memory. I am not guessing those.

**Owning expert:** `identity`

**INI-bound roles:** `rpm`

**Choices:**
- What does the ECU show? → `evidence.identity`
- Find related settings in the INI → `manual.identity`
- Review sensors and I/O → `tour.sensors`
- Continue: Trigger → `tour.trigger`

### `tour.sensors` — Sensors

**Seth:** Next, the sensors. We check which live channels exist, which readings are fresh, and which still need physical verification. A number on the screen does not prove the wiring is correct.

**Owning expert:** `sensors`

**INI-bound roles:** `clt`, `tps`, `map`, `lambda`

**Choices:**
- Review reported sensor evidence → `evidence.sensors`
- Find original ECU sensor settings → `manual.sensors`
- Continue Step 2: Trigger → `tour.trigger`
- Choose another topic → `targeted`

### `tour.trigger` — 02 / TRIGGER SETUP

**Seth:** Step 2: Trigger setup. We identify which crank/cam strategy the active configuration uses and whether the ECU reports useful synchronization diagnostics. That is not the same as verifying the physical timing reference, which comes later.

**Owning expert:** `trigger`

**INI-bound roles:** `sync`

**Choices:**
- Inspect available sync evidence → `evidence.trigger`
- Open relevant trigger settings → `manual.trigger`
- Continue Step 3: Base tables → `tour.base_tables`
- Get targeted trigger help → `topics.trigger`

### `tour.starting` — 05 / INITIAL STARTUP

**Seth:** Step 5: Initial startup evidence. We distinguish crank, catch, after-start and settled behavior as separate events. A sync error, unknown injector configuration or invalid sensor cannot be diagnosed by simply changing VE. I'll show what's observable and what requires a qualified check.

**Owning expert:** `starting`

**INI-bound roles:** `rpm`, `clt`

**Choices:**
- Review available start evidence → `evidence.starting`
- Explore start and warmup issues → `topics.start`
- Open original starting settings → `manual.starting`
- Continue Step 6: Idle → `tour.idle`

### `tour.idle` — 06 / IDLE

**Seth:** Step 6: Idle. We separate airflow, fueling, spark correction and feedback instead of treating every RPM fluctuation as bad PID. First we inspect the active idle strategy and measurements; then I can explain what's still unverified.

**Owning expert:** `idle`

**INI-bound roles:** `rpm`, `idleTarget`, `idleCommand`

**Choices:**
- Review idle measurements → `evidence.idle`
- Help with an idle symptom → `topics.idle`
- Find original idle settings → `manual.idle`
- Continue Step 7: Steady fuel → `tour.steady_fuel`

### `tour.warmup` — Warmup

**Seth:** A warm idle only tells us about warm idle. We need independent temperature-specific observations for cold-start and enrichment behavior.

**Owning expert:** `warmup`

**INI-bound roles:** `clt`

**Choices:**
- What does the ECU show? → `evidence.warmup`
- Find related settings in the INI → `manual.warmup`
- Continue: Base fueling and VE → `tour.fuel`
- Choose another topic → `targeted`

### `tour.fuel` — Base fueling and VE

**Seth:** We compare valid lambda evidence with target and the active fuel strategy. Closed-loop trims and transient enrichment can hide a bad base table, so those cases are not blindly counted.

**Owning expert:** `fuel`

**INI-bound roles:** `lambda`, `target`, `gego`

**Choices:**
- What does the ECU show? → `evidence.fuel`
- Find related settings in the INI → `manual.fuel`
- Continue: Fuel trims → `tour.trims`
- Choose another topic → `targeted`

### `tour.trims` — Fuel trims

**Seth:** Short-term and long-term trim are different signals with firmware-dependent scaling. I show each available bank separately and say when correction semantics are unknown.

**Owning expert:** `trims`

**INI-bound roles:** `gego`, `stft1`, `ltft1`

**Choices:**
- What does the ECU show? → `evidence.trims`
- Find related settings in the INI → `manual.trims`
- Continue: Throttle transitions → `tour.transients`
- Choose another topic → `targeted`

### `tour.transients` — Throttle transitions

**Seth:** Steady fueling and sudden throttle changes are different jobs. We do not teach VE from enrichment, fuel cut, or recovery events until they can be classified.

**Owning expert:** `transients`

**INI-bound roles:** `tps`, `map`

**Choices:**
- What does the ECU show? → `evidence.transients`
- Find related settings in the INI → `manual.transients`
- Continue: Ignition evidence → `tour.spark`
- Choose another topic → `targeted`

### `tour.spark` — Ignition evidence

**Seth:** Ignition timing affects torque and knock margin, but MAP, AFR and acceleration alone cannot establish best-torque timing. We can organize evidence and flag the need for instrumented verification.

**Owning expert:** `spark`

**INI-bound roles:** `spark`, `knock`

**Choices:**
- What does the ECU show? → `evidence.spark`
- Find related settings in the INI → `manual.spark`
- Continue: Optional ECU systems → `tour.advanced`
- Choose another topic → `targeted`

### `tour.advanced` — Optional ECU systems

**Seth:** Electronic throttle, boost, VVT, flex fuel, CAN and other extras only appear when the INI and current calibration show they apply. Missing support is shown, not made up.

**Owning expert:** `advanced`

**Choices:**
- What does the ECU show? → `evidence.advanced`
- Find related settings in the INI → `manual.advanced`
- Continue: Final review → `tour.validation`
- Choose another topic → `targeted`

### `tour.validation` — Final review

**Seth:** Now we audit what was really measured, what was only inferred, which settings changed, and what still requires qualified verification. I am not putting a fake 100%-tuned badge on this engine.

**Owning expert:** `validation`

**Choices:**
- What does the ECU show? → `evidence.validation`
- Find related settings in the INI → `manual.validation`
- Continue: Finish tour → `tour.done`
- Choose another topic → `targeted`

### `tour.done` — General

**Seth:** We've covered all ten chapters. Some findings may still need measurements or qualified verification; finishing a tour doesn't make the calibration complete. I live in this ECU, so I plan to be annoyingly clear about that.

**Choices:**
- Review unresolved evidence → `workspace.evidence`
- Get targeted help → `targeted`
- Browse INI settings → `atlas.entry`
- Guide home → `welcome`

### `topics.start` — Starting & warmup

**Seth:** Is it refusing to start, catching and then quitting, or just behaving differently hot and cold?

**Choices:**
- Cranks but does not catch → `issue.start.no_catch`
- Catches, then stops → `issue.start.catch_quit`
- Cold-start behavior → `issue.start.cold`
- Hot-start behavior → `issue.start.hot`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

### `issue.start.no_catch` — Cranks but does not catch

**Seth:** A crank-no-catch situation has several possible causes: synchronization, sensor validity, ignition reference or fueling. We should read the ECU fault and sync evidence before deciding a fuel table is wrong. What I can read right now: {{available:sync|rpm|clt}}.

**Owning expert:** `trigger`

**INI-bound roles:** `sync`, `rpm`, `clt`

**Choices:**
- Show relevant ECU readings → `evidence.trigger`
- Open the original ECU setting → `manual.trigger`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.start.catch_quit` — Catches, then stops

**Seth:** That transition can involve after-start fueling, idle airflow or other protections. We need a start-event timeline and verified sensors before assigning blame. What I can read right now: {{available:rpm|clt|idleCommand}}.

**Owning expert:** `starting`

**INI-bound roles:** `rpm`, `clt`, `idleCommand`

**Choices:**
- Show relevant ECU readings → `evidence.starting`
- Open the original ECU setting → `manual.starting`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.start.cold` — Cold-start behavior

**Seth:** Cold and warm starts are separate observations. We should compare start events by initial coolant temperature and wideband readiness; one event is not enough to tune a curve. What I can read right now: {{available:clt|rpm}}.

**Owning expert:** `warmup`

**INI-bound roles:** `clt`, `rpm`

**Choices:**
- Show relevant ECU readings → `evidence.warmup`
- Open the original ECU setting → `manual.warmup`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.start.hot` — Hot-start behavior

**Seth:** Heat-soak and warm-start conditions need their own evidence. We should check sensor plausibility and start history before assuming the cranking table is wrong. What I can read right now: {{available:clt|rpm}}.

**Owning expert:** `starting`

**INI-bound roles:** `clt`, `rpm`

**Choices:**
- Show relevant ECU readings → `evidence.starting`
- Open the original ECU setting → `manual.starting`
- Back to topics → `targeted`
- Guide home → `welcome`

### `topics.idle` — Idle

**Seth:** What part of idle is bothering you?

**Choices:**
- RPM surges or oscillates → `issue.idle.surge`
- Idle is too low or stalls → `issue.idle.low`
- Idle is too high → `issue.idle.high`
- Idle changes when loads appear → `issue.idle.load`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

### `issue.idle.surge` — RPM surges or oscillates

**Seth:** The big question is whether the idle actuator moves before the RPM does or follows it. Timing between signals helps separate unstable feedback from external disturbances. What I can read right now: {{available:rpm|idleCommand|idleTarget}}.

**Owning expert:** `idle`

**INI-bound roles:** `rpm`, `idleCommand`, `idleTarget`

**Choices:**
- Show relevant ECU readings → `evidence.idle`
- Open the original ECU setting → `manual.idle`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.idle.low` — Idle is too low or stalls

**Seth:** A low idle can involve insufficient airflow authority, load, incorrect sensor inputs or combustion behavior. Actuator saturation is evidence, not proof that a PID gain is bad. What I can read right now: {{available:rpm|idleCommand|idleTarget}}.

**Owning expert:** `idle`

**INI-bound roles:** `rpm`, `idleCommand`, `idleTarget`

**Choices:**
- Show relevant ECU readings → `evidence.idle`
- Open the original ECU setting → `manual.idle`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.idle.high` — Idle is too high

**Seth:** We should confirm whether the ECU is actually in idle mode and compare target RPM, commanded airflow and throttle feedback before touching the idle calibration. What I can read right now: {{available:rpm|idleCommand|idleTarget|tps}}.

**Owning expert:** `idle`

**INI-bound roles:** `rpm`, `idleCommand`, `idleTarget`, `tps`

**Choices:**
- Show relevant ECU readings → `evidence.idle`
- Open the original ECU setting → `manual.idle`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.idle.load` — Idle changes when loads appear

**Seth:** Load compensation might be involved, but we need load state, target RPM, actuator and ignition evidence together to explain why it changed. What I can read right now: {{available:rpm|idleTarget|idleCommand|spark}}.

**Owning expert:** `idle`

**INI-bound roles:** `rpm`, `idleTarget`, `idleCommand`, `spark`

**Choices:**
- Show relevant ECU readings → `evidence.idle`
- Open the original ECU setting → `manual.idle`
- Back to topics → `targeted`
- Guide home → `welcome`

### `topics.fuel` — Fuel & AFR

**Seth:** What part of fueling do you want to understand?

**Choices:**
- Measured lambda differs from target → `issue.fuel.lambda`
- Fuel trims keep correcting → `issue.fuel.trims`
- What does this VE table mean? → `issue.fuel.ve`
- Hesitation when throttle moves → `issue.fuel.throttle`
- Fueling changes with temperature → `issue.fuel.warm`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

### `issue.fuel.lambda` — Measured lambda differs from target

**Seth:** First I check whether the wideband is ready, whether the readings are current, and whether trim or enrichment is active. A mismatch during a transient is not a simple VE-table error. What I can read right now: {{available:lambda|target|lambdaGood|gego}}.

**Owning expert:** `fuel`

**INI-bound roles:** `lambda`, `target`, `lambdaGood`, `gego`

**Choices:**
- Show relevant ECU readings → `evidence.fuel`
- Open the original ECU setting → `manual.fuel`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.fuel.trims` — Fuel trims keep correcting

**Seth:** That could mean closed-loop compensation is covering an underlying error. I keep STFT, LTFT and Gego separate and will not assume they use the same neutral value. What I can read right now: {{available:gego|stft1|ltft1}}.

**Owning expert:** `trims`

**INI-bound roles:** `gego`, `stft1`, `ltft1`

**Choices:**
- Show relevant ECU readings → `evidence.trims`
- Open the original ECU setting → `manual.trims`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.fuel.ve` — What does this VE table mean?

**Seth:** Think of the VE table as a model indexed by the active RPM and load strategy. The INI tells us the actual shape; our evidence map is not a substitute for the original table editor. What I can read right now: {{available:rpm|map|lambda|target}}.

**Owning expert:** `fuel`

**INI-bound roles:** `rpm`, `map`, `lambda`, `target`

**Choices:**
- Show relevant ECU readings → `evidence.fuel`
- Open the original ECU setting → `manual.fuel`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.fuel.throttle` — Hesitation when throttle moves

**Seth:** Throttle changes can involve transient fueling and sensor delay. We need to separate those events from stable fueling; otherwise the VE learner will chase the wrong thing. What I can read right now: {{available:tps|map|lambda}}.

**Owning expert:** `transients`

**INI-bound roles:** `tps`, `map`, `lambda`

**Choices:**
- Show relevant ECU readings → `evidence.transients`
- Open the original ECU setting → `manual.transients`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.fuel.warm` — Fueling changes with temperature

**Seth:** Warmup fuel and steady-state fuel are not the same calibration. We should identify which temperature-based corrections are active before interpreting the lambda change. What I can read right now: {{available:clt|lambda|target}}.

**Owning expert:** `warmup`

**INI-bound roles:** `clt`, `lambda`, `target`

**Choices:**
- Show relevant ECU readings → `evidence.warmup`
- Open the original ECU setting → `manual.warmup`
- Back to topics → `targeted`
- Guide home → `welcome`

### `topics.sensors` — Sensors & wiring

**Seth:** Which signal looks suspicious?

**Choices:**
- Wideband or lambda → `issue.sensors.lambda`
- MAP or TPS → `issue.sensors.load`
- Coolant or intake temperature → `issue.sensors.temp`
- Fuel or oil pressure → `issue.sensors.pressure`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

### `issue.sensors.lambda` — Wideband or lambda

**Seth:** We should check whether the ECU declares the lambda channel valid, the value is fresh and the target comes from the right strategy. A plausible number alone does not guarantee sensor health. What I can read right now: {{available:lambda|lambdaGood|target}}.

**Owning expert:** `sensors`

**INI-bound roles:** `lambda`, `lambdaGood`, `target`

**Choices:**
- Show relevant ECU readings → `evidence.sensors`
- Open the original ECU setting → `manual.sensors`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.sensors.load` — MAP or TPS

**Seth:** We should compare MAP, TPS, RPM, and their configured input modes. Incorrect sensor scaling can look exactly like an incorrect fuel model. What I can read right now: {{available:map|tps|rpm}}.

**Owning expert:** `sensors`

**INI-bound roles:** `map`, `tps`, `rpm`

**Choices:**
- Show relevant ECU readings → `evidence.sensors`
- Open the original ECU setting → `manual.sensors`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.sensors.temp` — Coolant or intake temperature

**Seth:** Temperature readings influence starting, warmup, idle and protection logic. If the measurement is uncertain, dependent tuning results have to be reviewed. What I can read right now: {{available:clt}}.

**Owning expert:** `sensors`

**INI-bound roles:** `clt`

**Choices:**
- Show relevant ECU readings → `evidence.sensors`
- Open the original ECU setting → `manual.sensors`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.sensors.pressure` — Fuel or oil pressure

**Seth:** Pressure monitoring is important, but the available channel must be mapped to an actually installed and calibrated sensor. Missing hardware is not a zero reading. What I can read right now: {{available:fuelPressure|oilPressure}}.

**Owning expert:** `sensors`

**INI-bound roles:** `fuelPressure`, `oilPressure`

**Choices:**
- Show relevant ECU readings → `evidence.sensors`
- Open the original ECU setting → `manual.sensors`
- Back to topics → `targeted`
- Guide home → `welcome`

### `topics.trigger` — Trigger & sync

**Seth:** What does the ECU report or what needs verification?

**Choices:**
- Synchronization errors → `issue.trigger.no_sync`
- Ignition reference unclear → `issue.trigger.reference`
- Intermittent sync loss → `issue.trigger.intermittent`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

### `issue.trigger.no_sync` — Synchronization errors

**Seth:** We first check whether the ECU exposes sync status or error counters and whether the event is current. Physical wiring and trigger shape still require external verification. What I can read right now: {{available:sync|rpm}}.

**Owning expert:** `trigger`

**INI-bound roles:** `sync`, `rpm`

**Choices:**
- Show relevant ECU readings → `evidence.trigger`
- Open the original ECU setting → `manual.trigger`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.trigger.reference` — Ignition reference unclear

**Seth:** The commanded spark number is not proof of actual crankshaft timing. A qualified physical reference verification is required before treating ignition calculations as trustworthy. What I can read right now: {{available:spark|sync}}.

**Owning expert:** `trigger`

**INI-bound roles:** `spark`, `sync`

**Choices:**
- Show relevant ECU readings → `evidence.trigger`
- Open the original ECU setting → `manual.trigger`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.trigger.intermittent` — Intermittent sync loss

**Seth:** I would compare timestamped faults with RPM, battery and relevant conditions. The guide should not blame a tooth setting solely from one error count. What I can read right now: {{available:sync|rpm|battery}}.

**Owning expert:** `trigger`

**INI-bound roles:** `sync`, `rpm`, `battery`

**Choices:**
- Show relevant ECU readings → `evidence.trigger`
- Open the original ECU setting → `manual.trigger`
- Back to topics → `targeted`
- Guide home → `welcome`

### `topics.spark` — Ignition & knock

**Seth:** What are you trying to figure out about ignition?

**Choices:**
- Where should ignition timing be? → `issue.spark.timing`
- Knock or timing corrections → `issue.spark.knock`
- Ignition behavior at idle → `issue.spark.idle`
- Validate timing on a dyno → `issue.spark.verify`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

### `issue.spark.timing` — Where should ignition timing be?

**Seth:** Engine geometry and fuel characteristics help describe the problem, but they do not calculate a safe best-torque map. We can review the active spark strategy and what evidence is missing for validation. What I can read right now: {{available:spark|knock}}.

**Owning expert:** `spark`

**INI-bound roles:** `spark`, `knock`

**Choices:**
- Show relevant ECU readings → `evidence.spark`
- Open the original ECU setting → `manual.spark`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.spark.knock` — Knock or timing corrections

**Seth:** A reported knock event or retard needs source, RPM/load and correction context. The absence of reported knock does not prove timing can be advanced. What I can read right now: {{available:knock|spark|rpm|map}}.

**Owning expert:** `spark`

**INI-bound roles:** `knock`, `spark`, `rpm`, `map`

**Choices:**
- Show relevant ECU readings → `evidence.spark`
- Open the original ECU setting → `manual.spark`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.spark.idle` — Ignition behavior at idle

**Seth:** Idle spark may act as a fast stabilizer. We need to distinguish the commanded base advance from active corrections before interpreting RPM variation. What I can read right now: {{available:spark|rpm|idleTarget}}.

**Owning expert:** `idle`

**INI-bound roles:** `spark`, `rpm`, `idleTarget`

**Choices:**
- Show relevant ECU readings → `evidence.idle`
- Open the original ECU setting → `manual.idle`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.spark.verify` — Validate timing on a dyno

**Seth:** Instrumented torque evidence and controlled conditions are needed to make credible torque comparisons. The software can organize and explain that evidence without autonomously advancing timing. What I can read right now: {{available:spark|knock}}.

**Owning expert:** `validation`

**INI-bound roles:** `spark`, `knock`

**Choices:**
- Show relevant ECU readings → `evidence.validation`
- Open the original ECU setting → `manual.validation`
- Back to topics → `targeted`
- Guide home → `welcome`

### `topics.connection` — Connection & INI

**Seth:** What part of the connection is causing trouble?

**Choices:**
- Phone cannot reach BrickOS → `issue.connection.wifi`
- ESP does not see the ECU → `issue.connection.ecu`
- INI will not match → `issue.connection.ini`
- Readings stop updating → `issue.connection.stale`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

### `issue.connection.wifi` — Phone cannot reach BrickOS

**Seth:** Confirm the phone is on the ESP access point and that the browser can reach the device. The open AP is not authentication for automatic calibration writes.

**Owning expert:** `identity`

**Choices:**
- Show relevant ECU readings → `evidence.identity`
- Open the original ECU setting → `manual.identity`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.connection.ecu` — ESP does not see the ECU

**Seth:** The browser and ECU transport are separate. A working Wi-Fi page does not prove native USB is enumerated or that the ECU session is healthy.

**Owning expert:** `identity`

**Choices:**
- Show relevant ECU readings → `evidence.identity`
- Open the original ECU setting → `manual.identity`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.connection.ini` — INI will not match

**Seth:** The generated INI must match the live ECU firmware signature and layout; a similar name is not sufficient. We should inspect the definition status rather than guessing offsets.

**Owning expert:** `identity`

**Choices:**
- Show relevant ECU readings → `evidence.identity`
- Open the original ECU setting → `manual.identity`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.connection.stale` — Readings stop updating

**Seth:** A stale screen must say stale rather than showing a plausible old number. We should inspect current transport status before making any tuning judgment. What I can read right now: {{available:rpm}}.

**Owning expert:** `identity`

**INI-bound roles:** `rpm`

**Choices:**
- Show relevant ECU readings → `evidence.identity`
- Open the original ECU setting → `manual.identity`
- Back to topics → `targeted`
- Guide home → `welcome`

### `topics.more` — Other systems

**Seth:** Which of the remaining systems do you want to investigate?

**Choices:**
- Electronic throttle or idle actuator → `issue.more.dbw`
- Boost control → `issue.more.boost`
- Cam timing or VVT → `issue.more.vvt`
- CAN and auxiliary features → `issue.more.can`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

### `issue.more.dbw` — Electronic throttle or idle actuator

**Seth:** Electronic throttle is a safety-critical subsystem. The guide can locate its defined settings and diagnostics but cannot infer calibration validity from a single command or automatically perform actuator tests. What I can read right now: {{available:tps|idleCommand}}.

**Owning expert:** `advanced`

**INI-bound roles:** `tps`, `idleCommand`

**Choices:**
- Show relevant ECU readings → `evidence.advanced`
- Open the original ECU setting → `manual.advanced`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.more.boost` — Boost control

**Seth:** Boost calibration requires verification of the installed hardware, sensors and protection strategy. Unrecognized configurations should remain read-only. What I can read right now: {{available:map}}.

**Owning expert:** `advanced`

**INI-bound roles:** `map`

**Choices:**
- Show relevant ECU readings → `evidence.advanced`
- Open the original ECU setting → `manual.advanced`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.more.vvt` — Cam timing or VVT

**Seth:** Cam position targets and feedback must be correctly identified before analyzing control performance. Unknown mappings remain manual-review items. What I can read right now: {{available:rpm}}.

**Owning expert:** `advanced`

**INI-bound roles:** `rpm`

**Choices:**
- Show relevant ECU readings → `evidence.advanced`
- Open the original ECU setting → `manual.advanced`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.more.can` — CAN and auxiliary features

**Seth:** The active INI tells us which dialogs exist; the guide can point to them and explain their dependencies without inventing unavailable hardware. What I can read right now: {{available:rpm}}.

**Owning expert:** `advanced`

**INI-bound roles:** `rpm`

**Choices:**
- Show relevant ECU readings → `evidence.advanced`
- Open the original ECU setting → `manual.advanced`
- Back to topics → `targeted`
- Guide home → `welcome`

### `topics.unknown` — Help me figure it out

**Seth:** No problem. We can start from what the ECU actually reports. I will highlight stale inputs, current faults, missing calibration evidence and the original settings they relate to.

**Choices:**
- Show me the most important observation → `issue.unknown.overview`
- It has trouble starting → `issue.unknown.start`
- It runs but feels wrong → `issue.unknown.running`
- Back to help topics → `targeted`
- Full ECU tour → `tour.start`

### `issue.unknown.overview` — Show me the most important observation

**Seth:** I would begin by checking identity and fresh measurements. If those are missing, no engine-tuning diagnosis is reliable.

**Owning expert:** `sensors`

**Choices:**
- Show relevant ECU readings → `evidence.sensors`
- Open the original ECU setting → `manual.sensors`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.unknown.start` — It has trouble starting

**Seth:** Starting diagnostics distinguish sync, cranking, after-start and idle transitions; we will need a specific event before diagnosing. What I can read right now: {{available:rpm|sync}}.

**Owning expert:** `starting`

**INI-bound roles:** `rpm`, `sync`

**Choices:**
- Show relevant ECU readings → `evidence.starting`
- Open the original ECU setting → `manual.starting`
- Back to topics → `targeted`
- Guide home → `welcome`

### `issue.unknown.running` — It runs but feels wrong

**Seth:** First we separate stable fueling, transients, idle and ignition evidence. A general symptom alone cannot identify a bad calibration field. What I can read right now: {{available:rpm|lambda|map}}.

**Owning expert:** `fuel`

**INI-bound roles:** `rpm`, `lambda`, `map`

**Choices:**
- Show relevant ECU readings → `evidence.fuel`
- Open the original ECU setting → `manual.fuel`
- Back to topics → `targeted`
- Guide home → `welcome`

### `evidence.identity` — Evidence: ECU identity

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:rpm}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Owning expert:** `identity`

**INI-bound roles:** `rpm`

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.identity`
- Back to the guide → `back`

### `manual.identity` — Manual setting: ECU identity

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Owning expert:** `identity`

**INI-bound roles:** `rpm`

**Choices:**
- Open matching setting → `action.manual.identity`
- See available evidence → `evidence.identity`
- Back → `back`

### `evidence.sensors` — Evidence: Sensors

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:clt|tps|map|lambda}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Owning expert:** `sensors`

**INI-bound roles:** `clt`, `tps`, `map`, `lambda`

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.sensors`
- Back to the guide → `back`

### `manual.sensors` — Manual setting: Sensors

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Owning expert:** `sensors`

**INI-bound roles:** `clt`, `tps`, `map`, `lambda`

**Choices:**
- Open matching setting → `action.manual.sensors`
- See available evidence → `evidence.sensors`
- Back → `back`

### `evidence.trigger` — Evidence: Trigger and synchronization

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:sync}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Owning expert:** `trigger`

**INI-bound roles:** `sync`

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.trigger`
- Back to the guide → `back`

### `manual.trigger` — Manual setting: Trigger and synchronization

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Owning expert:** `trigger`

**INI-bound roles:** `sync`

**Choices:**
- Open matching setting → `action.manual.trigger`
- See available evidence → `evidence.trigger`
- Back → `back`

### `evidence.starting` — Evidence: First-start observations

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:rpm|clt}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Owning expert:** `starting`

**INI-bound roles:** `rpm`, `clt`

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.starting`
- Back to the guide → `back`

### `manual.starting` — Manual setting: First-start observations

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Owning expert:** `starting`

**INI-bound roles:** `rpm`, `clt`

**Choices:**
- Open matching setting → `action.manual.starting`
- See available evidence → `evidence.starting`
- Back → `back`

### `evidence.idle` — Evidence: Idle behavior

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:rpm|idleTarget|idleCommand}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Owning expert:** `idle`

**INI-bound roles:** `rpm`, `idleTarget`, `idleCommand`

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.idle`
- Back to the guide → `back`

### `manual.idle` — Manual setting: Idle behavior

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Owning expert:** `idle`

**INI-bound roles:** `rpm`, `idleTarget`, `idleCommand`

**Choices:**
- Open matching setting → `action.manual.idle`
- See available evidence → `evidence.idle`
- Back → `back`

### `evidence.warmup` — Evidence: Warmup

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:clt}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Owning expert:** `warmup`

**INI-bound roles:** `clt`

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.warmup`
- Back to the guide → `back`

### `manual.warmup` — Manual setting: Warmup

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Owning expert:** `warmup`

**INI-bound roles:** `clt`

**Choices:**
- Open matching setting → `action.manual.warmup`
- See available evidence → `evidence.warmup`
- Back → `back`

### `evidence.fuel` — Evidence: Base fueling and VE

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:lambda|target|gego}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Owning expert:** `fuel`

**INI-bound roles:** `lambda`, `target`, `gego`

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.fuel`
- Back to the guide → `back`

### `manual.fuel` — Manual setting: Base fueling and VE

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Owning expert:** `fuel`

**INI-bound roles:** `lambda`, `target`, `gego`

**Choices:**
- Open matching setting → `action.manual.fuel`
- See available evidence → `evidence.fuel`
- Back → `back`

### `evidence.trims` — Evidence: Fuel trims

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:gego|stft1|ltft1}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Owning expert:** `trims`

**INI-bound roles:** `gego`, `stft1`, `ltft1`

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.trims`
- Back to the guide → `back`

### `manual.trims` — Manual setting: Fuel trims

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Owning expert:** `trims`

**INI-bound roles:** `gego`, `stft1`, `ltft1`

**Choices:**
- Open matching setting → `action.manual.trims`
- See available evidence → `evidence.trims`
- Back → `back`

### `evidence.transients` — Evidence: Throttle transitions

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:tps|map}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Owning expert:** `transients`

**INI-bound roles:** `tps`, `map`

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.transients`
- Back to the guide → `back`

### `manual.transients` — Manual setting: Throttle transitions

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Owning expert:** `transients`

**INI-bound roles:** `tps`, `map`

**Choices:**
- Open matching setting → `action.manual.transients`
- See available evidence → `evidence.transients`
- Back → `back`

### `evidence.spark` — Evidence: Ignition evidence

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:spark|knock}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Owning expert:** `spark`

**INI-bound roles:** `spark`, `knock`

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.spark`
- Back to the guide → `back`

### `manual.spark` — Manual setting: Ignition evidence

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Owning expert:** `spark`

**INI-bound roles:** `spark`, `knock`

**Choices:**
- Open matching setting → `action.manual.spark`
- See available evidence → `evidence.spark`
- Back → `back`

### `evidence.advanced` — Evidence: Optional ECU systems

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Owning expert:** `advanced`

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.advanced`
- Back to the guide → `back`

### `manual.advanced` — Manual setting: Optional ECU systems

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Owning expert:** `advanced`

**Choices:**
- Open matching setting → `action.manual.advanced`
- See available evidence → `evidence.advanced`
- Back → `back`

### `evidence.validation` — Evidence: Final review

**Seth:** Here is what the matched ECU definition can expose for this system: {{available:}}. Missing or stale evidence stays unknown. You can open the detailed live evidence panel without leaving the guide.

**Owning expert:** `validation`

**Choices:**
- Open detailed evidence → `workspace.evidence`
- Find INI setting → `manual.validation`
- Back to the guide → `back`

### `manual.validation` — Manual setting: Final review

**Seth:** I can take you to the original INI-driven tuning editor. I will only offer a direct link if the matching INI actually resolves it. That editor is separate from this guide.

**Owning expert:** `validation`

**Choices:**
- Open matching setting → `action.manual.validation`
- See available evidence → `evidence.validation`
- Back → `back`

### `workspace` — Workspace

**Seth:** If you want all the evidence, tables and INI field inventory, the existing analysis workspace is still here. This conversation stays separate from Manual Tuning.

**Choices:**
- Open evidence → `workspace.evidence`
- Browse the INI Atlas → `workspace.atlas`
- View systems → `workspace.systems`
- Walk through INI settings → `atlas.entry`
- Guide home → `welcome`

### `atlas.entry` — General

**Seth:** The Configuration Atlas includes every calibration field the active INI parser can decode. We can inspect them one by one, or search the original INI tree. If I don't know what a setting does, I'll tell you instead of making up an answer.

**Choices:**
- Walk through decoded settings → `field.0`
- Search the full INI Atlas → `workspace.atlas`
- Back to guide home → `welcome`

### `tour.configuration` — 01 / ECU CONFIGURATION

**Seth:** Step 1: We start with what the ECU is configured to control. The matching INI reveals its field definitions and live channels, not whether every wire or sensor has been physically verified. I'll sort the knowns from the assumptions before we go further.

**Owning expert:** `identity`

**INI-bound roles:** `rpm`, `battery`

**Choices:**
- Check ECU identity and evidence → `evidence.identity`
- Review inputs and sensor configuration → `tour.sensors`
- Open ECU settings → `manual.identity`
- Continue Step 2: Trigger → `tour.trigger`

### `tour.base_tables` — 03 / BASE TABLES

**Seth:** Step 3: Base tables. Before we interpret fueling or spark behavior, we need to know which fuel model and ignition tables are actually active. I can find matching INI entries; I cannot declare an unknown starting map correct just because it exists.

**Owning expert:** `fuel`

**INI-bound roles:** `rpm`, `map`, `target`

**Choices:**
- Find original fuel table settings → `manual.fuel`
- Find original ignition settings → `manual.spark`
- Inspect existing fuel evidence → `evidence.fuel`
- Continue Step 4: Reference checks → `tour.reference`

### `tour.reference` — 04 / PHYSICAL REFERENCES

**Seth:** Step 4: Reference checks. The ECU can report ignition angle and fuel pressure if the matching INI provides those channels. Physical timing and fuel-system behavior still require qualified external verification. I'm trapped in the ECU, not carrying a timing light.

**Owning expert:** `spark`

**INI-bound roles:** `spark`, `fuelPressure`

**Choices:**
- Review reported reference channels → `evidence.reference`
- Open original ignition settings → `manual.spark`
- I need help interpreting a sensor → `topics.sensors`
- Continue Step 5: Starting → `tour.starting`

### `evidence.reference` — REFERENCE EVIDENCE

**Seth:** Here is what the ECU can actually report: {{available:spark|fuelPressure|rpm}}. A reported reading is not independent confirmation of a physical timing mark or fuel-pressure measurement. Unavailable signals are recorded as unknown.

**Owning expert:** `spark`

**INI-bound roles:** `spark`, `fuelPressure`, `rpm`

**Choices:**
- Open ignition settings → `manual.spark`
- Return to reference checks → `tour.reference`
- Open evidence workspace → `workspace.evidence`

### `tour.steady_fuel` — 07 / STEADY FUEL

**Seth:** Step 7: Steady-state fuel. The active fuel strategy, valid measured/target lambda, closed-loop trims and time-aligned operating conditions determine what the evidence means. A tidy lambda number can be compensation, not a correct VE table. We review findings first; nothing writes itself.

**Owning expert:** `fuel`

**INI-bound roles:** `lambda`, `target`, `gego`, `stft1`, `ltft1`

**Choices:**
- Inspect current fuel evidence → `evidence.fuel`
- Understand trims and warmup → `tour.fuel_support`
- Open original fuel settings → `manual.fuel`
- Continue Step 8: Ignition review → `tour.steady_spark`

### `tour.fuel_support` — FUEL SUPPORT TOPICS

**Seth:** Fuel trims, warmup and throttle transitions need their own explanations even though the main tour has ten steps. We keep their evidence separate so the VE learner doesn't mistake one compensation for another.

**Owning expert:** `fuel`

**INI-bound roles:** `lambda`, `target`, `gego`, `stft1`, `ltft1`

**Choices:**
- Inspect STFT and LTFT → `tour.trims`
- Warmup and after-start → `tour.warmup`
- Throttle transients → `tour.transients`
- Back to steady-state fuel → `tour.steady_fuel`

### `tour.steady_spark` — 08 / STEADY IGNITION

**Seth:** Step 8: Steady-state ignition. We'll identify the active ignition table and reported corrections, then make clear what the sensors can and cannot establish. Knock absence, MAP and acceleration are not proof of best-torque timing; that needs proper measurement and professional review.

**Owning expert:** `spark`

**INI-bound roles:** `spark`, `knock`

**Choices:**
- Inspect spark/knock evidence → `evidence.spark`
- Open original ignition settings → `manual.spark`
- Help me understand ignition → `topics.spark`
- Continue Step 9: Instrumented review → `tour.full_power`

### `tour.full_power` — 09 / INSTRUMENTED REVIEW

**Seth:** Step 9: Full-power validation is a separate professional, instrumented stage—not an automatic test initiated by this guide. I can organize the ECU's reported faults and available evidence, but I won't infer that a high-load calibration is safe or complete from missing warnings.

**Owning expert:** `validation`

**INI-bound roles:** `knock`, `fuelPressure`, `oilPressure`

**Choices:**
- Review available ECU evidence → `evidence.validation`
- Review optional system configuration → `tour.advanced`
- See unresolved diagnostic findings → `workspace.systems`
- Continue Step 10: Confirm results → `tour.confirm`

### `tour.confirm` — 10 / FINAL CONFIRMATION

**Seth:** Step 10: Confirm the tune. We compare what was actually verified with what remains unknown: ECU identity, physical checks, measured operating regions, diagnostic faults, and any changes since the last assessment. Finishing this conversation does not certify the engine.

**Owning expert:** `validation`

**INI-bound roles:** `rpm`, `lambda`, `sync`

**Choices:**
- Review evidence and exclusions → `workspace.evidence`
- Review full INI coverage → `workspace.atlas`
- Review final findings → `evidence.validation`
- Finish the full tour → `tour.done`

## Maintenance rules

1. Edit the canonical JSON catalogue on ESP, not in DS source.
2. Increment the catalogue version for semantic/branching changes and preserve stable node IDs.
3. Run full link, ten-stage path, INI mismatch and no-write regression tests.
4. Regenerate this Markdown from the same JSON and regenerate firmware web assets before building.
5. DS will consume future ESP-provided dialogue packets without compiling ECU-specific offsets into its app.

**Scope:** Read-only guidance; not a calibration write or engine operating procedure.

## 30 chapter-by-chapter question dialogues

The complete current chapter question prompts, explanations, INI role bindings and response paths are in [Seth Chapter Questions](28-seth-chapter-questions.md). Edit the versioned catalogue, not this generated Markdown, to change the guide. User-reported answers are not independently verified calibration results.
