# Seth's reasoning engine v1 — evidence and hypotheses rather than scripted guesses

**Implementation status:** Phone browser prototype, read-only. Firmware v4.8 still supplies ECU telemetry, matched INI, HTTP and USB; the diagnostic calculations currently run **in the phone browser**, not in a dedicated ESP task. The original 3DS, DSi, and manual tuning interfaces are unchanged. No ignition advance, ECU RAM changes, flash burns, commands, or automatic physical procedures are implemented.

## What changed

Seth is more than an INI textbook now. His Full Tour and Targeted Help pages can call a common deterministic **diagnostic reasoner**. This evaluates a short, bounded history of the *actual INI-resolved* readings and gives:

- **Facts**: what the ECU reported, with source and freshness. This does not certify physical sensor accuracy.
- **Patterns**: repeated variations across the current observation window (not proof of independent visits or reproducible engine behavior).
- **Hypotheses**: multiple plausible explanations for a symptom, never an unsupported single-cause declaration.
- **Contradictions**: e.g., lambda near target while closed-loop Gego corrects away from neutral.
- **Missing inputs**: exact INI roles or live channel readings not available; unknown stays unknown.
- **Next discriminating question**: asks for data that would best differentiate competing explanations. Weighting is a transparent prioritization heuristic, *not* a probabilistic causal model.
- **Uncertainty / boundaries**: highlights nonphysical confirmation limits and what cannot be concluded.

Dialogue generated from this analysis is already returned in the existing portable Seth **packet**, with a structured `reasoning` object. A future DS client can render those server-computed words and fields when the reasoning engine is ported to ESP firmware, without compiling per-INI questions into the DS executable. This ESP-side move is **not implemented yet**.

## Live data and interpretation rules

The existing `GuidedAnalyzer` already resolves fields using the live ECU-matched INI and independently validated output-channel lengths. It now holds a bounded maximum **160 samples or 30 seconds** of INI-decoded role values, with time-based decimation to at most one history point per 100 ms. The browser does not poll the ECU more often just because Seth analyzes the current window.

An unsupported signal, invalid numeric value, unmatched INI, stale report or missing signal cannot be silently converted into zero. For hard validity failures the reasoner stops diagnostic inference and prioritizes identity/telemetry recovery. The reasoner labels its diagnostic patterns as ECU-reported observations; hardware health and true mechanical causes require independent verification.

## Current expert rules

| Expert | Evidence considered | What Seth does / does not infer |
| --- | --- | --- |
| Definition/connection | ECU signature, matching INI, fresh telemetry | Stops interpreting if mismatch or stale data |
| Sensors | WBO2 readiness, coolant time behavior | Flags invalid lambda and repeated implausible jumps as *possible* sensor/electrical symptoms |
| Trigger/starting | RPM and INI-resolved trigger error-counter trend | Notices increases; does not guess physical trigger installation faults |
| Idle | Actual RPM, target RPM, reported actuator position and history | Recognizes persistent target deviations and RPM variation; lists airflow, control, load, fueling and ignition alternatives |
| Base fuel / trims | Measured and target lambda, Gego, relevant flags | Identifies target-masking by feedback; does not treat lambda at target as proof of VE calibration |
| Transient response | Available RPM/TPS/load/lambda channels | Identifies missing prerequisites; never trains steady VE on unsupported state assumptions |
| Spark | Actual spark, knock-related signals, temperatures | Explicitly refuses to infer MBT or recommend advance from absent knock or MAP/accelerometer evidence alone |
| Warmup / advanced / final | Availability, physical verification requirements | Frames missing evidence instead of fabricating results |

The rules currently use a single time window and limited available channels. They are a **first engine**, not proven causal inference. More advanced methods must be gated behind repeatable bench/replay evidence, instrumented hardware tests, active strategy resolution and stronger timestamp correction.

## Diagnostic packet for future DS portability

Example schema, illustrative only:

```json
{
  "schema": "brickos.guide.reasoning.v1",
  "stage": "idle",
  "freshness": "live",
  "confidence": "limited evidence",
  "facts": [ { "code": "rpm", "text": "ECU reports ...", "source": "ecu" } ],
  "hypotheses": [ {
    "id": "idle-target-mismatch", "kind": "hypothesis",
    "observation": "...", "causes": ["..."],
    "missing": ["..."], "confidence": "window-pattern"
  } ],
  "contradictions": [],
  "missing_inputs": [],
  "questions": [ { "id": "idleCommand", "prompt": "...", "method": "ecu" } ],
  "read_only": true
}
```

The packet contains **render-ready dialogue plus structured evidence**, not raw INI offsets guessed on the device. The 3DS client should never independently choose a different hypothesis for the same ECU state. If the firmware doesn't support a semantics rule, Seth should say that and link to the original manual editor.

## Dialogue and control behavior

From Full Tour or Quick Help, pressing **Seth, investigate this using ECU evidence** opens a diagnostic page. The next page contains facts, multiple explanations, contradictions, missing measurements and the priority question. **What would distinguish the causes?** opens an explanation of the next supported question. **Inspect live evidence** opens the original guided evidence view; **Open related original ECU setting** links to the existing manual INI tree where a resolvable menu entry exists. Navigation never modifies calibration or restores permissions to write.

## Further logic milestones

1. Use ECU mode flags to verify *active* idle, after-start, enrichment and fuel-cut modes (INI declaration alone is insufficient).
2. Add explicit sensors with timestamp uncertainty, measured signal quality and calibrated engineering units.
3. Model independent visits, cross-condition repetition, hysteresis and change-point detection; currently one time window cannot establish reproducibility.
4. Track stable diagnostic hypotheses and source-revision IDs across sessions, with false-positive / false-negative replay datasets.
5. Separate model error from hardware fault, external load, and user-reported physical checks as independent evidence classes.
6. Audit every expert for unsupported assumptions and provide a documented “not enough evidence” outcome.
7. Move matching expert logic and render-ready dialogue from browser to ESP, with a single versioned API for both phone and 3DS.
8. Perform ESP-IDF build and physical read-only ECU bench validation before treating these as deployed features.

## Tests

`node --test tests/*.test.mjs` from `firmware/esp32` covers the new reasoner plus existing browser tests. `bash tests/run_host_syntax.sh` and `bash tests/run_assets_host.sh` cover available host checks. Passing host tests is not proof of hardware correctness.
