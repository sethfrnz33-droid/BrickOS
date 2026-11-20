# Seth Agent — Standalone ESP-portable prototype 0.2

**This folder is NOT linked into the BrickOS ESP-IDF build.** It does not alter the ESP firmware, phone tuner, DS/DSi applications, CAN/USB transport, or ECU commands. It runs offline as a separate code experiment.

## Implemented in the prototype

- Ten-stage HP Academy-inspired course navigation with original BrickOS teaching content.
- A build-time generator compiles all 22 authored, versioned lesson concepts from `design/education/lesson-catalog.v1.json` into static C records; no JSON parsing or internet connection is needed at runtime.
- Four explanation depths, independently selectable learning/check preferences, an alternate explanation for 'I don't understand', optional knowledge checks with shuffled answer positions, and progress tracked by the existing heap-free C education-state module.
- Progressive user-reported hardware questions: PFI/GDI/mixed, pump controller, regulator, and pressure sensing. PWM pump control **never implies** a missing mechanical regulator. GDI and mixed injection fuel guidance are explicitly blocked.
- Read-only symptom classification for starting, idle, fuel/lambda, trigger/sync, sensors and ignition, always describing uncertainty and missing evidence; no actual ECU observations are present.
- Small, bounded C response packets with original Seth dialogue, explanation, choices and explicit evidence status, suitable for a later phone or DS renderer.

## Standalone build and tests

From the repository root, with Python 3 and a C11 compiler:

```bash
cd prototype/seth-agent
make test
make
./build/seth_demo
```

The generated build/seth_catalog.c is **local build output only**, not part of the ESP firmware. The build links `prototype/esp_education_core.c` (the existing bounded educational state) as a separate dependency.

## Deferred until explicit integration

ESP-IDF build/linking, authenticated profile persistence, exact INI role mapping, live telemetry, high-speed evidence recording, automatic VE estimates, active ECU writes, source-driven diagnosis, phone UI wiring, and DS integration are NOT implemented. The 2026-11-09 Proteus F7 generated INI remains the initial supported baseline; its declaration is not a live ECU/hardware verification.

The eventual deployment requires measured flash/heap/stack budgets and separate communication/security validation. Educational answers are never hardware certification or permission to perform engine control operations.

**No code in this folder starts an engine, tests outputs, changes ignition/fuel tables, writes RAM or burns flash.**

## New in V0.2 — working coordinator and reasoned evidence

**Coordinator:** `seth_coordinator.c` evaluates the current ten-stage chapter and chooses a genuinely available next lesson, the next missing user-reported hardware fact, or a non-automatable evidence/qualified-verification task. Every stage remains **unverified** without external evidence. Reading a lesson, correctly answering its optional quiz, or reporting that hardware was checked does not imply calibration or physical readiness.

**Prerequisite-aware tutor:** `seth_learning_path.c` walks the versioned, cycle-checked prerequisite graph. For example, asking about VE interpolation can first teach ECU/INI definitions, active tables, sensor readiness, targets and fuel trims, then return to the requested concept. `study_target` in the packet carries that breadcrumb through an interruption.

**Offline UI action router:** `seth_actions.c` accepts allowlisted commands (`study_topic`, `continue_topic`, `full_tour`, `stage`, `build`, `set_fact`, `explain_alternate`, `check_start`, `check_answer`, `depth`, `interaction`, `diagnose`, `report`, `home`). It rejects stale expected dialog revisions and unknown commands. **This is not authentication and grants no write authority.**

**Portable local checkpoint:** `seth_checkpoint.c` serializes educational state, engine-build answers, dialogue revision and active study topic into a bounded, little-endian, CRC32-checked byte buffer. Decoding is atomic with caller-supplied scratch memory. No operating-system file I/O, flash persistence or network access is included. **Any ECU-definition match is intentionally cleared on restore**; the next production implementation must revalidate connected hardware/INI independently.

**Synthetic evidence reviewer:** `seth_evidence.c` compares bounded, explicitly attributed replay samples with caller-selected timestamp-skew limits. It distinguishes sensor readiness, active fuel trims, reported synchronization, changing error counters, idle target-versus-actual availability and capture gaps. It reports *observations and missing evidence* without establishing physical causes or generating tune values. The 128-sample review buffer is **not** a high-rate event recorder.

**Portable dialogue:** `seth_packet_json.c` includes schema, response state, dialogue revision, active original topic and reasoned stage task. Phone/DS clients will eventually render these packets; they do not need to run the expert logic.

### Commands

```bash
cd prototype/seth-agent
make clean && make test && make
./build/seth_demo
./build/seth_demo --json
```

The V0.2 tests include more than 1,000 interrupted/restarted learning-session roundtrips, damaged-checkpoint rejection, GDI/mixed injection scope checks, curriculum prerequisite sequencing, stale dialogue action rejection, and offline sensor-evidence replay checks. These are **host tests only**, not actual ESP-IDF/ECU validation.

### Integration rule

No file in this folder belongs to the production `firmware/BrickOS/main/CMakeLists.txt` build; CI checks that boundary. The ESP's current CAN/USB bridge and manual tuner remain unchanged. Any future persistence, live sensor source, INI mapping, actuator or ECU writing interface requires a **separate** integration review and target hardware testing.
