# Seth's Engine Build Archive — Progressive Questions and Phone UI V1

> **Design only**. The current phone browser has Seth's versioned dialogue but no persistent ESP-owned physical engine archive. The separate `prototype/engine-profile-policy.mjs` is a pure, read-only rule experiment. It does not communicate with or modify the ECU. Do not treat an owner's answer as independent mechanical or electrical verification.

## Core requirement

Seth maintains **one project profile per physical engine**, independent of the ECU software revision. He uses it to choose which sections of the Full Tour apply, how to explain a sensor or fuel issue, which questions to ask, and which evidence can legitimately support *review-only* autotune proposals.

The archive is never a forced registration form. A user who enters Quick Help sees a useful guide immediately; Seth gathers missing facts as needed, updates the archive, and returns to the same problem. `I don't know` is a valid answer. The profile contains no real names, passwords or sensitive personal data.

## Phone home

```
┌──────────────────────────────────────────┐
│ BRICKOS  |  Ask Seth          ECU: LIVE  │
│ Engine profile: Project A   [edit]       │
├──────────────────────────────────────────┤
│  [pixel Seth]                            │
│ "What are we sorting out today?"         │
│                                          │
│ [ Full Guided Tour ]  [ Quick Help ]     │
│ [ Assess current ECU ] [ Review event ] │
│ [ Your engine build ]  [ Past cases ]    │
├──────────────────────────────────────────┤
│ PROFILE FACTS                            │
│ Injection: PFI (user reported)           │
│ Pump: PWM (user reported)                │
│ Regulator: Unknown — question queued    │
│ Pressure data: Unknown                   │
│                                            │
│ NEXT RELEVANT QUESTION [Explain why]      │
├──────────────────────────────────────────┤
│ Manual Tune | Evidence | Logs             │
└──────────────────────────────────────────┘
```

No entire form blocks entry. Profile has `KNOWN / REPORTED / ECU-CONFIGURED / OBSERVED / DOCUMENTED / UNKNOWN / CONFLICTED` indicators. It does **not** use a universal `safe` indicator.

## Seth's progressive fuel-system conversation

This original conversation illustrates the logic. Actual displayed options depend on the archive and active INI.

Seth: "Before we talk about how to model fuel, what's actually putting fuel into the engine? Port injectors, direct injectors, a combination, or not sure?"
- `Port injection (PFI)` → continue PFI archive and INI verification.
- `Direct injection (GDI)` or `Both` → **GDI not supported**. No injector mapping, pressure strategy or fuel autotune instructions; preserve the build facts and allow generic read-only ECU inspection.
- `Not sure` → explain that unsupported/unverified injection hardware blocks fuel-specific guidance; offer non-fuel menu and archive help.

Seth: "How is your pump being commanded? On/off relay, PWM-capable driver, external OEM controller, or unknown?"
- If `PWM`: "That only tells me about the command. How is rail pressure actually regulated? A mechanical reference, electronic pressure feedback, both, or you don't know yet?"
- If `relay`: likewise ask regulation when relevant; a relay doesn't establish a fixed pressure strategy.
- If `external controller`: preserve unknowns; do not claim the ECU directly knows pressure.

Seth: "Does a pressure sensor report actual rail pressure to the ECU, or do you have a separately documented pressure measurement?"
- If `ECU sensor`: check the matched INI for its calibration and reference type (absolute/gauge/differential), and source freshness/validity. An INI-defined sensor is not a validated real sensor.
- If `external measurement`: archive that it was supplied and the context of the report; no continuous ECU pressure display is invented.
- If `none/unknown`: mark physical pressure verification unresolved and block dependent fuel-estimation claims.

Seth: "I know what pressure model this ECU is configured to use. That doesn't tell me whether the physical fuel system matches it. Here's the mismatch, if any."

This logic must treat **PWM pump plus mechanical regulator** as an ordinary, possible combination. If no regulator is confirmed, pressure is not declared correct merely because the pump is variable speed.

## What the archive stores

**Engine identity:** opaque project ID, optional label and broad engine family, user-provided relevant dimensions, cylinder count, aspiration and other features as they become relevant. Not required for a generic question.

**Fuel architecture:** PFI/GDI/mixed/unknown, configured ECU injection mode (separate), injector characterization source, fuel type/composition information, pump driver/controller type, pressure regulator/reference, pressure instrumentation and validity. Do not conflate `fuel pressure is controlled` with `fuel pressure is verified`.

**Sensors and hardware:** input capabilities inferred from matched INI vs actual installed sensor information, hardware assignments, confirmed physical signal references, instrument availability, potential dual-bank/wideband architecture and unknowns.

**Evidence provenance:** each fact contains `source`, `verification`, `evidence_ref`, `updated_at`, and profile revision; each task or diagnostic finding links to the revision used to reach it.

**Consent and retention:** explicit opt-in to local persistence; import/export and deletion. Current open AP means no promise of confidentiality. Browser storage may be used as a temporary local-only prototype, but production multi-device consistency should be owned by an authenticated ESP store with versioned backup/export and bounded storage, not by a single phone localStorage key.

## Applying the archive to the ten stages

| HPA-inspired chapter | Build facts Seth needs (when relevant) | What can be automatically reviewed |
| --- | --- | --- |
| 1. ECU configuration / I-O | Injection type, cylinder/injector/ignition hardware, sensor presence | INI/ECU mismatch, unsupported strategy, missing inputs |
| 2. Trigger | Known trigger/cam sensor types and verified reference | ECU sync state/error history, timing data gaps |
| 3. Base maps | Fuel algorithm, injector data, aspiration and fuel type | Active table selection, definitions/units, revision history |
| 4. Ignition / pressure | Independent timing reference, pump controller, regulation and pressure reference | Known versus unknown physical checks and ECU pressure channel status |
| 5. Initial startup | Relevant build context plus ECU observed state | Start-event and fault review (no engine commands) |
| 6. Idle | Idle actuator strategy and installed load-control features | RPM/target/actuator/trim consistency |
| 7. Steady fuel | PFI support confirmed, valid active fueling strategy, wideband/pressure evidence | Qualifying lambda/trim observations and review-only proposed trends |
| 8. Steady ignition | Physical timing confirmation and qualified evidence | Current spark/correction provenance; no MBT prediction |
| 9. Full power | Independent test instrumentation and qualified operator evidence | Report comparisons only; no automated power tests |
| 10. Confirmation | Profile conflicts resolved or explicitly unresolved, archived revisions | Final evidence audit, not a blanket claim of safe tuning |

No force-advance from one stage simply because an explanation has been read. Each stage tracks (a) article/dialogue read, (b) owner self-reported review, (c) matched ECU evidence, and (d) independently documented verification separately.

## A specific concrete PFI decision case

Profile declares: PFI, PWM pump, regulator unknown, no pressure sensor information.
- Seth asks regulation **without** inferring no regulator.
- He can explain the rusEFI pump-control page if it is present in this INI.
- He cannot infer physical rail pressure, assume fixed differential injector pressure, or declare the VE learning context validated.
- Engine-profile chapter may continue into non-fuel explanations while fuel validation remains blocked.

Profile declares: PFI, PWM pump, manifold-referenced mechanical regulator, ECU fuel pressure sensor.
- Seth retains **both** control and regulation facts.
- He distinguishes actual measured/reference pressure from inferred pressure and the injector compensation model.
- Missing independent verification remains labeled missing; there is no automatic pump controller or pressure calibration.

Profile declares: PFI, electronically regulated returnless system with validated pressure sensing and independently documented pressure strategy.
- Seth may show pressure-target versus observation **only if the selected rusEFI/ECU firmware actually exposes the relevant pressure-control strategy, native units and active status**.
- Recorded consistency under a measured window does not prove the entire fuel delivery system correct under all conditions.

Profile declares: GDI, mixed injection, or conflicting injection architecture.
- Show **Not supported by BrickOS Guided Fuel Tuning**.
- Never silently coerce it into PFI, regardless of whether the matching upstream rusEFI INI happens to contain a GDI setting.

## Compatibility with INI updates, phone and DS

A changed ECU/INI fingerprint:
1. preserves physical engine archive facts and their evidence references,
2. remaps roles through the new *verified* INI only,
3. detects contradictory configuration and marks impacted experts as `NEEDS_RECONCILIATION`,
4. invalidates calibration inferences tied to old tables or scales,
5. sends updated portable Seth tasks/dialogue packets to the phone and later DS.

The browser UI displays original manual settings by existing INI-derived paths. It must not embed physical engine assumptions in the DS binary. The future DS acts only as a renderer of versioned packets, not a second independent tuner.

## Acceptance

1. Unknown profile opens and Quick Help works without creating fabricated hardware.
2. The first relevant unknown fact appears as **one** short question.
3. PWM without regulator answer does **not** automatically select `no regulator`.
4. GDI and mixed values produce an explicit unsupported message.
5. A new INI can invalidate one inference without deleting the archived engine name/injection hardware description.
6. No user-reported review becomes `externally verified` without a recorded independent source.
7. The same profile + INI snapshot + evidence yields the same answer on phone and future DS.
8. Changing profile values produces a revision event and refreshes affected diagnoses.
9. No build-profile action issues ECU commands, changes the tune, or burns flash.
10. The existing `Manual Tune`, 3DS/DSi network interfaces and ECU transport are untouched.

## References
- [Unified tuning coordinator](33-unified-tuning-coordinator.md)
- [Diagnostic agent](30-seth-diagnostic-agent-master-plan.md)
- [Engine archive contract](../design/engine-build/profile-contract.v1.json)
- [Read-only archive policy prototype](../prototype/engine-profile-policy.mjs)
- [HP Academy published ten stages](https://www.hpacademy.com/courses/practical-standalone-tuning/)
- [rusEFI fuel pump](https://github.com/rusefi/rusefi/wiki/Fuel-Pump)
- [rusEFI fuel-system pressure reference](https://wiki.rusefi.com/Fuel-System/)
