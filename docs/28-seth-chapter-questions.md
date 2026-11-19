# Seth — Complete Chapter Question Dialogue

**Source of truth:** `firmware/esp32/web/guide-catalog.json` version `1.2.0`. This document is a review copy, not a second source of dialogue.

**Current implementation:** read-only phone browser guide. No autonomous tuning, RAM edits, flash writes, ignition advancement, or console-side guide implementation is claimed.

**Portability:** The ESP serves the catalogue; current browser interpretation returns structured packets containing `tour_step`, `check`, `chapter_progress`, `choices`, ECU definition identity, available INI-derived telemetry, and optional original-editor targets. The DS/DSi should render the same packets once the ESP-side resolver is implemented. New INI fields do not require rewriting the authored dialogue or console binary.

## Conversation mechanics

`tour.<chapter>` → `check.<chapter>.<zero-based-index>` → `answer.<chapter>.<index>.reported` / `.unknown`. `why.<chapter>.<index>` provides an explanation. Every chapter has three authored questions.

**Response labels:** I reviewed the evidence / I checked it independently; I don't know / not checked; Why does this matter?; Show measured evidence; Open related original setting when the matching INI defines a menu target; Next question.

**Interpretation:** `reported` is **only a user statement**, never a verified sensor or hardware result; `unknown` remains unresolved. No answer changes ECU settings. Missing or stale signals are identified as unknown.

## Full authored dialogue

### 1. ECU configuration and testing

**Chapter entry:** `tour.configuration`; **Expert:** `identity`.

#### Question 1: `matching_ini`

**Seth:** Do the ECU signature and generated INI match?

**Why Seth asks:** Every live reading and setting depends on the correct definition.

**INI-bound evidence roles:** `rpm`

**Original manual category:** `identity` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.


#### Question 2: `engine_inputs`

**Seth:** Have the configured sensors and engine inputs been checked against the actual installation?

**Why Seth asks:** Configured values cannot independently verify physical wiring.

**INI-bound evidence roles:** `rpm`, `clt`, `map`, `tps`

**Original manual category:** `sensors` (link only if matched INI resolves it).

**External verification:** Required; cannot be established solely from ECU-reported values.


#### Question 3: `backup`

**Seth:** Is there a recoverable baseline calibration backup?

**Why Seth asks:** A browser export is not automatically a validated ECU backup.

**INI-bound evidence roles:** None — external/user context required

**Original manual category:** `identity` (link only if matched INI resolves it).

**External verification:** Required; cannot be established solely from ECU-reported values.

### 2. Trigger setup

**Chapter entry:** `tour.trigger`; **Expert:** `trigger`.

#### Question 1: `sync_reporting`

**Seth:** Can we establish whether trigger synchronization data is available?

**Why Seth asks:** An unavailable channel is not evidence of a fault-free trigger.

**INI-bound evidence roles:** `rpm`, `sync`

**Original manual category:** `trigger` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.


#### Question 2: `trigger_configuration`

**Seth:** Does the configured trigger setup agree with the installed hardware?

**Why Seth asks:** Plausible RPM alone does not establish correct crank or cam setup.

**INI-bound evidence roles:** `rpm`, `sync`

**Original manual category:** `trigger` (link only if matched INI resolves it).

**External verification:** Required; cannot be established solely from ECU-reported values.


#### Question 3: `physical_reference`

**Seth:** Has the ignition reference been independently verified?

**Why Seth asks:** Commanded spark readings do not prove physical reference alignment.

**INI-bound evidence roles:** `spark`, `sync`

**Original manual category:** `spark` (link only if matched INI resolves it).

**External verification:** Required; cannot be established solely from ECU-reported values.

### 3. Base table configuration

**Chapter entry:** `tour.base_tables`; **Expert:** `fuel`.

#### Question 1: `fuel_model`

**Seth:** Which fuel strategy and base table are actually active?

**Why Seth asks:** INI table names alone do not establish which fuel strategy is used.

**INI-bound evidence roles:** `map`, `tps`, `lambda`, `target`

**Original manual category:** `fuel` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.


#### Question 2: `injectors`

**Seth:** Have injector and fuel-system assumptions been reviewed?

**Why Seth asks:** Incorrect injector assumptions can be masked by other corrections.

**INI-bound evidence roles:** `fuelPressure`

**Original manual category:** `fuel` (link only if matched INI resolves it).

**External verification:** Required; cannot be established solely from ECU-reported values.


#### Question 3: `spark_model`

**Seth:** Which base ignition strategy and correction sources are active?

**Why Seth asks:** Reported actual timing can contain multiple corrections.

**INI-bound evidence roles:** `spark`, `knock`

**Original manual category:** `spark` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.

### 4. Base ignition / fuel pressure verification

**Chapter entry:** `tour.reference`; **Expert:** `spark`.

#### Question 1: `ignition_reference`

**Seth:** Is the physical timing reference independently confirmed?

**Why Seth asks:** The ECU cannot verify its own external timing reference solely from reported data.

**INI-bound evidence roles:** `spark`, `rpm`

**Original manual category:** `spark` (link only if matched INI resolves it).

**External verification:** Required; cannot be established solely from ECU-reported values.


#### Question 2: `fuel_pressure`

**Seth:** Is fuel pressure independently verified when required?

**Why Seth asks:** Missing pressure instrumentation must remain unknown, not normal.

**INI-bound evidence roles:** `fuelPressure`

**Original manual category:** `fuel` (link only if matched INI resolves it).

**External verification:** Required; cannot be established solely from ECU-reported values.


#### Question 3: `protection_review`

**Seth:** Have the applicable ECU protection and sensor-fault settings been reviewed?

**Why Seth asks:** No warning currently displayed does not establish that protections were validated.

**INI-bound evidence roles:** `knock`, `oilPressure`, `fuelPressure`

**Original manual category:** `sensors` (link only if matched INI resolves it).

**External verification:** Required; cannot be established solely from ECU-reported values.

### 5. Initial startup

**Chapter entry:** `tour.starting`; **Expert:** `starting`.

#### Question 1: `start_state`

**Seth:** Can the ECU distinguish cranking and after-start states?

**Why Seth asks:** State recognition matters before interpreting an initial-start log.

**INI-bound evidence roles:** `rpm`, `sync`, `clt`

**Original manual category:** `starting` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.


#### Question 2: `start_history`

**Seth:** Have multiple comparable startup events been recorded?

**Why Seth asks:** One event does not validate a temperature-dependent start strategy.

**INI-bound evidence roles:** `rpm`, `clt`, `battery`

**Original manual category:** `starting` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.


#### Question 3: `faults`

**Seth:** Have startup faults and hardware uncertainties been reviewed?

**Why Seth asks:** A hardware issue cannot be safely diagnosed as a VE problem.

**INI-bound evidence roles:** `sync`, `battery`

**Original manual category:** `sensors` (link only if matched INI resolves it).

**External verification:** Required; cannot be established solely from ECU-reported values.

### 6. Idle tuning

**Chapter entry:** `tour.idle`; **Expert:** `idle`.

#### Question 1: `control_strategy`

**Seth:** Do we know the enabled idle-control strategy?

**Why Seth asks:** An idle valve, throttle and other strategies have different controls.

**INI-bound evidence roles:** `idleTarget`, `idleCommand`

**Original manual category:** `idle` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.


#### Question 2: `baseline_response`

**Seth:** Does comparable idle evidence reveal how RPM relates to commanded control?

**Why Seth asks:** Variation may involve air, fuel, spark, feedback, load or hardware.

**INI-bound evidence roles:** `rpm`, `idleTarget`, `idleCommand`

**Original manual category:** `idle` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.


#### Question 3: `fuel_and_corrections`

**Seth:** Can we distinguish idle fueling from active corrections?

**Why Seth asks:** Target lambda under closed-loop correction does not prove correct idle VE.

**INI-bound evidence roles:** `lambda`, `target`, `gego`, `stft1`, `spark`

**Original manual category:** `fuel` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.

### 7. Steady-state fuel

**Chapter entry:** `tour.steady_fuel`; **Expert:** `fuel`.

#### Question 1: `lambda_ready`

**Seth:** Are target and measured lambda valid, fresh and scaled correctly?

**Why Seth asks:** Unknown units or unready sensors invalidate an inferred VE error.

**INI-bound evidence roles:** `lambda`, `target`, `lambdaGood`

**Original manual category:** `fuel` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.


#### Question 2: `trims_interpretation`

**Seth:** Do we understand how active STFT, LTFT and Gego affect this observation?

**Why Seth asks:** Fuel-feedback scaling and bank applicability must not be guessed.

**INI-bound evidence roles:** `gego`, `stft1`, `stft2`, `ltft1`, `ltft2`

**Original manual category:** `trims` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.


#### Question 3: `steady_evidence`

**Seth:** Do the actual fuel-table regions have independent stable observations?

**Why Seth asks:** Transient or repeated samples do not automatically count as independent coverage.

**INI-bound evidence roles:** `rpm`, `map`, `tps`, `lambda`

**Original manual category:** `fuel` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.

### 8. Steady-state ignition

**Chapter entry:** `tour.steady_spark`; **Expert:** `spark`.

#### Question 1: `active_spark`

**Seth:** Does the INI expose the active ignition table and related corrections?

**Why Seth asks:** Actual spark is not necessarily the base table value.

**INI-bound evidence roles:** `spark`, `knock`

**Original manual category:** `spark` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.


#### Question 2: `fuel_conditions`

**Seth:** Are fueling and temperature conditions known for these observations?

**Why Seth asks:** Different conditions can make two ignition observations incomparable.

**INI-bound evidence roles:** `lambda`, `target`, `clt`, `map`

**Original manual category:** `spark` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.


#### Question 3: `torque_evidence`

**Seth:** Is there appropriate independent torque evidence for any MBT claim?

**Why Seth asks:** MAP, acceleration and knock absence alone cannot establish MBT.

**INI-bound evidence roles:** `spark`, `knock`

**Original manual category:** `spark` (link only if matched INI resolves it).

**External verification:** Required; cannot be established solely from ECU-reported values.

### 9. Full-power instrumented review

**Chapter entry:** `tour.full_power`; **Expert:** `validation`.

#### Question 1: `instrumentation`

**Seth:** Are professional supervision and external measurements documented?

**Why Seth asks:** This review organizes instrumented evidence; it never commands an engine test.

**INI-bound evidence roles:** `fuelPressure`, `knock`

**Original manual category:** `validation` (link only if matched INI resolves it).

**External verification:** Required; cannot be established solely from ECU-reported values.


#### Question 2: `operating_envelope`

**Seth:** Which operating regions have actually been evaluated with qualifying evidence?

**Why Seth asks:** Unmeasured operating areas remain unknown, not verified.

**INI-bound evidence roles:** `rpm`, `map`, `lambda`

**Original manual category:** `validation` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.


#### Question 3: `protective_events`

**Seth:** Are reported warnings and protective events included in the review?

**Why Seth asks:** No logged warning is not proof that an operating condition is safe.

**INI-bound evidence roles:** `knock`, `oilPressure`, `fuelPressure`

**Original manual category:** `validation` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.

### 10. Final confirmation

**Chapter entry:** `tour.confirm`; **Expert:** `validation`.

#### Question 1: `open_findings`

**Seth:** Are unresolved faults and evidence gaps visible in the final summary?

**Why Seth asks:** A final report must not hide unknowns or unverified hardware.

**INI-bound evidence roles:** `sync`, `lambda`

**Original manual category:** `validation` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.


#### Question 2: `revision_check`

**Seth:** Does the recorded ECU definition and calibration revision still match?

**Why Seth asks:** Changes can invalidate earlier evidence and conclusions.

**INI-bound evidence roles:** `rpm`

**Original manual category:** `identity` (link only if matched INI resolves it).

**External verification:** Not necessarily required for this question; data validity still matters.


#### Question 3: `record_results`

**Seth:** Is the review record separate from ECU saving and flashing?

**Why Seth asks:** Finishing the tour is not a flash burn or certification of the engine.

**INI-bound evidence roles:** None — external/user context required

**Original manual category:** `validation` (link only if matched INI resolves it).

**External verification:** Required; cannot be established solely from ECU-reported values.

## Device-independent data contract

```json
{
  "schema": "brickos.guide.dialog.v1",
  "catalog_version": "1.2.0",
  "node_id": "check.configuration.0",
  "tour_step": 1,
  "tour_total": 10,
  "definition_key": "example:matched-INI-fingerprint",
  "freshness": "stale",
  "check": {
    "index": 1,
    "total": 3,
    "status": "unknown",
    "status_meaning": "Needs review; no correctness claim",
    "requires_external": false
  },
  "chapter_progress": {
    "total": 3,
    "reported": 0,
    "unknown": 1,
    "unreviewed": 2
  },
  "manual_target": null,
  "read_only": true
}
```

The example is illustrative, not a live ECU reading. Response packet labels, status and prompts are generated from the shared versioned catalogue and current matched INI; clients must not embed fixed calibration offsets or assume a channel exists.

**Current limitation:** Phone check answers are cached **in that browser**, scoped to the combined ECU identity/INI fingerprint and catalogue version. Cross-device answer synchronization and ESP-owned semantic validation have not yet been implemented. The DS app and its existing communication remain unchanged.

## Publishing and testing

Update the canonical catalogue, validate node links and question rules, rebuild ESP browser assets with `python firmware/esp32/tools/generate_web_assets.py`, then run `node --test firmware/esp32/tests/*.test.mjs` and host checks. The ESP-IDF build regenerates embedded assets at configuration time. Physical bench verification is still required. No copyrighted course lessons are reproduced.
