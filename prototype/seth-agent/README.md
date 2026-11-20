# Seth Agent — Standalone ESP-portable prototype 0.1

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
