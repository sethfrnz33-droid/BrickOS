# BrickOS Verified Research Ledger — rusEFI, TunerStudio, HPA and ESP-IDF

**Research pass:** November 19, 2026. **Status: source-backed design; no ECU firmware updated.** Public upstream source and documentation can change after this review. This file records what those sources establish; a **live, exact-match generated INI and actual ECU firmware build** still have to be inspected before the implementation can claim any field/feature works on Brick ECU.

## 1. Source classes and trust hierarchy

| Class | Meaning | Used for | Does *not* prove |
| --- | --- | --- | --- |
| Repo current source | Code in current BrickOS `main`, anchored by commit/version | What the imported ESP/browser actually implements | Validated behavior on physical hardware |
| rusEFI upstream generator | TunerStudio template, config definitions, LiveData source | How a particular build *might* generate fields, pages and expressions | That the user's ECU exposes an identical field |
| Exact generated INI | Signature-matched, cached and checksum-verified build artifact | Real type, scaling, page, offset, shape, menu/dialog visibility and output roles | Correct physical sensor/wiring or active mechanical hardware |
| Live ECU snapshot | Communication, decoded configuration and timestamped outputs | Configured/runtime state, reported faults, observed corrections | That commands reached physical hardware or measurements are independently accurate |
| Engine archive | Versioned user-reported hardware details and externally documented checks | Applicability/branching for this exact build | That any user assertion is independently true |
| External calibration evidence | Qualified, independently identified measurements, with dates/revisions | Narrow validation of what was actually tested | Universal safety, optimization or roadworthiness |
| HPA public education | Ten-stage conceptual ordering and public explanations | Pedagogical sequence, prerequisites and what to distinguish | Right to copy paid lessons or substitute for professional evaluation |

This hierarchy is **not a simple ranking**: upstream INI documentation and physical-hardware evidence answer different questions. An INI can tell Seth how a channel is encoded; it cannot prove the sensor is attached correctly.

## 2. Verified rusEFI configuration-generation facts

**Upstream:** https://github.com/rusefi/rusefi/blob/master/firmware/tunerstudio/tunerstudio.template.ini
and https://github.com/rusefi/rusefi/blob/master/firmware/integration/rusefi_config.txt

- The template contains `[TunerStudio]` signature and communication settings, `[Constants]` page metadata, a generated `@@TS_PAGE_FIELD_BLOCKS@@` expansion, and additional menus, tables and visibility expressions. `rusefi_config.txt` defines persistent structure fields; runtime output-channel metadata is linked to generated LiveData definitions.
- The template's page list is **generated for enabled pages** and can omit disabled/zero-size pages; it explicitly warns that page ordinals may change if e.g. LTFT is disabled. **Never hardcode page indices** from an unrelated INI. Read all actual generated `nPages`, `pageIdentifier`, `pageSize`, read/write command strings and endianness, validating their internal consistency.
- The template advertises settings page `0x0000`, scatter `0x0100`, LTFT `0x0200`, second tables `0x0300`, and Lua `0x0400` as **possible page IDs**. These are not proof all pages exist on every ECU. A runtime page-read success is also not proof its contents have the semantics expected by a different firmware version.
- `noMsqSave` display aliases (e.g. WUE analyzer views of lambda/CLT curves, AFR-vs-lambda target adders) can reference the **same bytes** as another named setting. **Do not inventory or automatically propose edits twice** just because the INI contains two names at the same address.
- Unit displays and scale expressions can switch according to user-selected settings (e.g. AFR vs lambda). A semantically resolved numeric source records native representation and display transform separately, with the actual context of the expression.
- The config generator describes migration of renamed, scaled, resized or reinterpreted fields. An engine archive and educational progress can survive a firmware revision; **ECU calibration evidence mapped to the old field cannot be reused without explicit compatible migration and revalidation**.
- `[SettingGroups]`, conditional INI blocks, dialog field visibility, tables, info labels and buttons are separate semantic types; a visible button may issue a special command, whereas a readout is not editable. The Guide must distinguish all these when deep-linking into the existing Manual Tuning app.
- The template's `defaultRuntimeRecordPerSec=100` is a TunerStudio preference, **not** a guaranteed ECU data delivery rate or trigger-edge sampling resolution.

**TunerStudio public developer specification:** https://tunerstudio.com/index.php/support/manuals/tsdevmanuals , https://www.tunerstudio.com/index.php/support/manuals/tsdevmanuals/57-conditional-ini-blocks-and-using-settinggroups-in-ini-files and https://www.tunerstudio.com/index.php/products/tuner-studio/tsarticles/140-configuring-dynamically-sized-tables-in-tunerstudio

The engine needs a **parsing-feature support matrix**: numeric constants, arrays, multidimensional tables, bits, enums, conditions, macros, transforms, unsaved aliases, dynamic dimensions, lookup-dependent expressions, complex nested dialogs, command buttons, info/indicator-only items, disabled fields and unknown tokens. Unsupported expressions must be quarantined, **not evaluated using arbitrary code or guessed values**.

## 3. Verified operating-state and telemetry distinctions

**Upstream:** https://wiki.rusefi.com/Logging-Guide/ and https://wiki.rusefi.com/Trigger/

The documented `engine` output channel is a **bitmask**, not a single exclusive state. It identifies running vs cranking, after-start correction, warmup correction, acceleration-related and deceleration/fuel-cut-related conditions; multiple bits can coexist. The upstream documentation explicitly distinguishes this from **Engine Mode**, which describes configured fuel/injection/ignition algorithms.

**Crucial consequence:** an analyzer must record:
- primary state (source-defined, may be unknown),
- simultaneous correction/transition overlays (not a single exclusive enum),
- source freshness, actual sampling interval and time uncertainty,
- active calibration strategy and last known revision,
- sensor readiness and validity,
- missing/unverified flags, and explicit reason for excluding each observation.

A trigger tooth logger / engine sniffer / ordinary TunerStudio log / ECU SD log are **different evidence products**. Ordinary 100 Hz output-channel polling cannot reconstruct tooth-edge timestamps. A logger's ability to show tooth intervals depends on firmware/build/protocol support and must be verified separately, not inferred from the presence of a `triggerErrorCounter` name.

Case events should pair **event timestamps, corresponding tune, ECU firmware/INI, source type and data-quality counters**. Events captured at different configurations or temperature/load states are not automatically comparable.

## 4. Fuel model, pump pressure and trim distinctions

**Upstream:**
- https://wiki.rusefi.com/Fuel-System/
- https://wiki.rusefi.com/Fuel-Pressure/
- https://wiki.rusefi.com/Fuel-Pump/
- https://wiki.rusefi.com/Fuel-Overview/
- https://github.com/rusefi/rusefi/blob/master/docs/AI/fueling_system.md

Verified concepts:
- Injector delivery depends on fuel pressure **relative to the injector's reference**. The upstream configuration distinguishes injector compensation mode, fuel-pressure-sensor mode and fuel reference pressure. A known pump command isn't measured rail pressure.
- rusEFI fuel-pump control can be relay, two relay stages or PWM pressure-control, depending on ECU build/configuration. A PWM-capable pump driver can coexist with a mechanical regulator, independent OEM controller or sensor. **Ask controller, regulator, sensor, reference and independent validation as separate facts.**
- The documented PWM pressure-control system has feedforward/base-duty, optional measured-pressure feedback, target and correction state; **missing pressure feedback affects what can be concluded about actual pressure**. Do not assume this mode is present merely because a generated INI contains its settings.
- Fueling is a multi-stage model: target lambda, base air/fuel model, VE, charge-temperature and barometric compensation, transient fueling, closed-loop STFT, learned LTFT when enabled, injector characteristics, and engine protection. Correctly measured lambda can conceal active feedback correction. **No universal idle/cruise stoichiometric target** should be hardcoded as a health verdict.
- Some runtime features conditionally pause learning, apply fuel cuts or change target lambda. The exact applicability of a correction requires **matched build + active flags + source timestamps**; unrecognized compensation means no confident steady-state VE conclusion.
- GDI and mixed GDI/PFI are **explicitly unsupported by BrickOS Guided Fuel Tuning**, even when upstream rusEFI has optional GDI implementations. Broad wiki capability cannot override Brick ECU product policy.

### Evidence table for explanations

| User question | What the ECU may establish | What remains outside |
| --- | --- | --- |
| "Is the fuel pump okay?" | Configured control mode, requested duty/relay state and measured pressure **if a verified channel exists** | Actual wiring/controller, hydraulic system, pressure under conditions not measured |
| "Is AFR perfect?" | Active target, measured lambda, sensor validity, active trim/correction and state | Accuracy of sensor calibration and unobserved conditions |
| "Does this VE cell need adjusting?" | Map/axis membership, qualified observations and non-executable statistical estimate | Correct injector/pressure model, independence of evidence, external validation |
| "Why does idle wander?" | RPM/target/actuator/spark/fuel timing patterns **if available** | Confirmation of physical actuator movement, hidden loads, singular root cause |
| "Is ignition optimal?" | Reported actual spark/corrections, warning channels and qualitative context | Independently verified best-torque timing, all mechanical risks |

## 5. HPA curriculum and learning research

**Public HPA:** https://www.hpacademy.com/courses/practical-standalone-tuning/getting-started-the-hpa-10-step-process/ ; https://www.hpacademy.com/courses/practical-standalone-tuning/ .

The publicly described sequence is ECU configuration and I/O, trigger, base tables, physical timing/pressure references, initial startup, idle, steady fuel, steady ignition, full-power work, and final confirmation. HPA explicitly frames it as manageable tasks and notes that calibration needs appropriate instrumentation. BrickOS may adopt the **general order**, but not copy course lessons, paid materials or promised results.

**Learning evidence:** https://ies.ed.gov/ncee/WWC/PracticeGuide/1/Published discusses spaced/retrieval practice for long-term learning. Implement **optional, kind, concept-only knowledge checks**, with explanations for wrong answers and revisit suggestions; tests of understanding cannot substitute for hardware/ECU validation. Seth's four explanation depths and separate interaction preference prevent trapping novices in mandatory quizzes.

## 6. ESP32-S3 storage, performance and security

**ESP-IDF sources:**
- https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32s3/api-reference/system/heap_debug.html
- https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32s3/api-guides/file-system-considerations.html
- https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/partition-tables.html
- https://docs.espressif.com/projects/esp-idf/en/release-v5.3/esp32s3/api-guides/wifi-security.html

The current project's `sdkconfig.defaults` selects 4 MB flash; `partitions.csv` contains 24 KiB NVS, 2.5 MiB factory app, and ~1.44 MiB logs SPIFFS. **They are allocations, not evidence of available free bytes**. No OTA slot or separate learning archive partition is allocated. Check actual compiled app size, largest available heap block, stack margins and optional PSRAM presence before adding more content.

Espressif recommends NVS for low-frequency configuration/key-value data; high-frequency large logs should not be written there. The default NVS partition is also used by runtime components, and frequent updates amplify wear/storage pressure. A small, versioned/correctly checksummed learner checkpoint is not the same storage product as a high-rate evidence ring.

Current `main.c` hardcodes an open Wi-Fi AP. Even if HTTPS/WPA is technically supported by ESP-IDF, **the present device is not securely paired**, and the guide cannot equate connected Wi-Fi with operator authorization. Do not deploy autonomous ECU writes under a tutorial session token or assume learning-progress actions grant write privileges.

## 7. What still requires verification **before** implementation

1. Capture **one actual matching generated INI** and ECU signature/layout from the target Brick firmware; verify exact enabled/disabled pages, byte order, field types/offsets/conditions, and runtime channel interpretations. This research inspected upstream templates, **not a complete current Brick generated INI capture**.
2. Compare the user's hardware variant and installed engine/injector/pressure/sensor architecture with actual ECU software configuration; classify all unverified points.
3. Inspect the complete INI parser against the public TunerStudio specification; build fixture tests for macros, aliases, conditionals and dynamic tables rather than inferring support from one test case.
4. Measure real ESP USB polling data rate/timeout/multi-block skew, TunerStudio coexistence, and optional tooth-stream support. `100 Hz` as a requested rate cannot certify delivered throughput.
5. Inspect exact board PSRAM/flash availability and measured compiled binary/storage footprint; select a safe archive retention/migration strategy.
6. Verify the complete capability registry with `supported | disabled | unverified | not present | unsupported` statuses. Only verified capabilities can become actionable tutorial suggestions.
7. Independently review any future calibration control; this research is informational and **does not authorize physical tests, live changes, performance runs or GDI support**.

## Status terminology for source assertions

`CONFIRMED_UPSTREAM_DOC`: upstream source asserts the feature/concept.  
`CONFIRMED_CURRENT_SOURCE`: code exists in this repository.  
`REQUIRES_TARGET_INI`: exact fields/options depend on the connected ECU.  
`REQUIRES_HARDWARE_CHECK`: can't be established from ECU output alone.  
`REQUIRES_MEASUREMENT`: performance/timing/storage fact cannot be claimed from a preferred setting.  
`PROPOSED`: newly designed behavior, not integrated code.

When authoring new Seth dialogue, every sentence that names a numeric ECU value, hardware part, sensor source or capability should inherit one of these statuses.
