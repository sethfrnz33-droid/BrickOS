# BrickOS — complete Brick ECU software workspace

**One repository for the ESP32-S3 firmware, Nintendo 3DS Brick Tuner Suite software, and Guided Tuning research/prototypes.** It is not exclusively an autotuner repository.

| Location | Role | Current status |
|---|---|---|
| [`firmware/BrickOS/`](firmware/BrickOS/) | ESP32-S3 Wi-Fi/USB bridge and embedded browser tuner | **v4.8 runtime string / imported firmware lineage**, with later browser and calibration-command changes; ESP-IDF and physical ECU validation not established |
| [`apps/3ds/`](apps/3ds/) | Brick Tuner Suite native Nintendo 3DS source (app README identifies v6.5), assets, build scripts and tests | **Imported**; host tests 42/42 pass; native build/hardware not yet verified |
| [`apps/dsi/`](apps/dsi/) | Brick Tuner Suite Nintendo DSi source (app README identifies v29), tests and SD sound assets | **Imported and subsequently updated in repo**; host tests exist, native hardware verification outstanding |
| [`prototype/`](prototype/) | Standalone Guided Tuning Python reference implementation | Existing read-only simulation, not deployable ESP firmware |
| [`docs/`](docs/) | Guided Tuning design + installation, connections and firmware architecture | Research, specifications and setup instructions |
| [`ROADMAP.md`](ROADMAP.md) | Cross-project engineering roadmap | Planning only |

## ESP32 installation and connections

Start with **[ESP32 installation and hardware connections](docs/ESP32_INSTALL_AND_CONNECTIONS.md)**, then **[firmware architecture](docs/ESP32_FIRMWARE_ARCHITECTURE.md)**. The uploaded firmware targets the **ESP32-S3** with **ESP-IDF 5.3.1**, communicates with rusEFI using native USB CDC (not CAN), and exposes Wi-Fi AP `brick` at `http://192.168.4.1/` with TCP port `2000`. Its optional GPIO, USB power and SD details are described in the installation guide.

### Build on ESP-IDF 5.3.1

```powershell
cd firmware\BrickOS
python tools/generate_web_assets.py
idf.py set-target esp32s3
idf.py build
idf.py -p COM7 flash monitor
```

Use your verified ESP32 programming COM port in place of the **example** `COM7`. Do not flash this image to an STM32 ECU. For security, use the open Wi-Fi AP only on an isolated bench.

## Guided Tuning

- [Source-reviewed full-system gap audit, release blockers and implementation priorities](docs/35-full-system-gap-audit.md)
- [Seth Full Tour — ten-stage sequence and DS-portable INI contract](docs/27-hpa-master-tour.md)

- [HP Academy public 10-step workflow comparison and recommended Guide refinements](docs/26-hp-academy-process-comparison.md)

### Current planning specifications (research/design, not implemented firmware)

- [ESP autotune runtime, analysis filters, ownership and controller lease](docs/19-autotune-runtime-and-controls.md)
- [Full INI-driven guided calibration journey and reasoning decisions](docs/20-guided-calibration-journey.md)
- [3DS two-screen Guided Tuning navigation and wireframes](docs/21-3ds-guided-layouts.md)
- [Phone browser Guided Tuning responsive layouts](docs/22-phone-guided-layouts.md)
- [Shared ESP/3DS/phone state contract and acceptance tests](docs/23-guided-tuning-protocol-and-acceptance.md)
- [Earlier UX specification](docs/10-user-experience.md) and [calibration proposal / review transaction model](docs/18-transaction-and-review.md)

Guided Tuning is a **separate, opt-in mode** designed to discover the active ECU/INI configuration, learn from valid telemetry, explain calibration dependencies, and make evidence-backed recommendations. It must never impede manual tuning or silently change ECU flash. The phone-first **read-only Guided Tuning** browser workspace is now integrated under `firmware/BrickOS/web/`, with [implementation notes](firmware/BrickOS/README_GUIDED_PHONE_V1.md). It uses the existing ESP USB live sampler and runs its learning decisions in the phone browser; the ESP-resident autonomous controller and automatic ECU changes remain unimplemented. The existing Python simulator under `prototype/` remains independent.

## Source provenance

The `firmware/BrickOS/` source is copied from `BrickOS-v48-TRIGGER-INI-FIX(2).zip` as supplied 2026-11-19; runtime source and embedded assets are unchanged. Its historical `README.md` (which referenced v4.2) is preserved as `README_LEGACY_V42.md` and replaced with the current setup README. The actual 3DS v6.3 and DSi v2.2 sources are now imported under `apps/`; integrating the proposed Guided Tuning controller into ESP firmware and client apps remains future work.


## Seth's Guide — phone-first, INI-dependent

**One guide data source, multiple future displays.** The phone now has a separately navigable **Ask Seth** experience (full ECU tour, targeted troubleshooting, field-by-field INI walkthrough), using the user's mascot portrait and gibberish audio. The story content lives in `firmware/BrickOS/web/guide-catalog.json` and is served by the ESP at **`GET /api/guide/v1/catalog`**. Seth is the lightly exasperated ECU-bound clone of the creator; he explains settings and diagnostics without interrupting Manual Tune.

- [Full authored Seth dialogue and branching choice script](docs/24-seth-complete-dialogue.md)
- [INI-bound, portable phone/DS dialogue contract and explicit limitations](docs/25-portable-ini-guide-contract.md)
- [Phone implementation, build, routes, media, and tests](firmware/BrickOS/README_SETH_GUIDE_V2.md)

**Compatibility:** No 3DS or DSi source or connection protocol was changed for this guide. A later ESP-hosted role/state service must produce resolved view packets for the DS, so changing the INI or Seth's lines does not require updating console code. The current phone resolves roles in the browser from the verified INI; this is a **read-only tutorial**, not ECU-resident automatic calibration or ignition tuning.

## Full Guided Tour chapter questions

Seth now has [30 INI-aware questions across the HP Academy-ordered ten-chapter tour](docs/28-seth-chapter-questions.md). The ESP-served catalogue is authoritative; phone currently records browser-local user-reported answers, **not** independent ECU verification. DS/DSi code and connection paths are unchanged.

## Seth reasoning diagnostics — read-only browser feature

Seth can now [investigate matched ECU telemetry and distinguish observations from competing hypotheses](docs/29-seth-evidence-reasoning-v1.md) during Full Tour or Quick Help. The portable packet includes facts, hypotheses, contradictions, missing INI channels, and the best next diagnostic question. This is **read-only browser-side inference**, not the ESP-resident calibration/autotune engine, not automatic ignition advance, and not physical verification. Current 3DS/DSi and manual tuning integrations are unchanged.

## Seth Diagnostic Agent — high-rate event capture master plan

**Planning and design registry only — not new deployed firmware functionality.** The current Seth reasoner is still browser-side and the ESP has no dedicated high-rate pre/post event recorder. Actual ECU data rate, buffer capacity and tooth-level capture support must be measured/verified before implementation. DS/DSi connections and existing Manual Tuning remain unchanged.

- [Diagnostic agent master plan: Full Tour, Quick Help, current-state assessments and multi-cause investigations](docs/30-seth-diagnostic-agent-master-plan.md)
- [High-rate event recorder: pre/post capture, ECU timestamp quality, buffer/storage, USB scheduling and faults](docs/31-seth-high-rate-capture-and-evidence-spec.md)
- [Phone case UI, Seth dialogue flows, prompt parsing, portable output for future DS](docs/32-seth-diagnostic-case-ux-and-reasoning-flows.md)
- [Machine-readable 57-case design registry — **not loaded by current firmware**](design/diagnostics/scenario-registry.v1.json)

The proposed scenario registry and fallback policy have host-side validation under `firmware/BrickOS/tests/seth-scenario-registry.test.mjs`; **unit tests do not validate a physical ECU diagnosis**. All future sensor-role bindings must derive from the active verified INI, ECU configuration and verified expert rules. The guide must never replace a qualified external check with a guessed value or treat a momentary "on target" lambda as proof an entire engine is optimal.

## Unified Seth + Autotune architecture and engine archive

**Planned, not deployed:** Seth coordinates HP Academy-inspired chapters, local engine-build facts, matched rusEFI INI capabilities, diagnostic cases and review-only autotune evidence; real ESP-owned profile persistence and an authorized live autotuning controller remain unimplemented. **GDI and mixed PFI/GDI are explicitly unsupported** in BrickOS Guided Fuel Tuning, regardless of upstream rusEFI features. PWM pump control never implies the presence or absence of a regulator, and does not independently establish correct rail pressure.

- [Unified ten-stage tuning coordinator: manual vs analysis vs future automation](docs/33-unified-tuning-coordinator.md)
- [Progressive engine-build archive questions, phone layout, and Seth dialogue](docs/34-engine-build-archive-and-seth-intake.md)
- [Versioned local engine archive data contract and PFI-only capability policy](design/engine-build/profile-contract.v1.json)
- [Offline read-only engine profile policy prototype](prototype/engine-profile-policy.mjs)
- [Profile policy regression tests](firmware/BrickOS/tests/engine-profile-policy.test.mjs)

Engine hardware facts are keyed to a local engine profile, **not** to a volatile INI fingerprint; ECU layout changes require revalidation of dependent data without deleting reported build information. The prototype is purely explanatory and cannot write ECU RAM/flash or control a pump/engine. The open Wi-Fi AP is not an authenticated archive service; an ESP-owned persistent store would require a separately validated security and consent design.

## Seth's ESP-resident educational master plan

**Scope and status:** Planning, authored starter content and **host-tested pure-C state prototype**, not yet linked into the ESP-IDF firmware. The current browser dialogue remains phone-side; we have **not** shipped an ESP-side adaptive instructor, full engine archive, high-rate case recorder, authorized autotuner or DS guide.

- [Complete product specification: adaptive teaching, HPA-ordered calibration guide and evidence separation](docs/36-seth-complete-teaching-and-tuning-master-spec.md)
- [Full dialogue behavior, educational checks, onboarding, and phone/DS screen workflows](docs/37-seth-education-screen-and-dialogue-flows.md)
- [ESP32-S3 resource budgets, task scheduling, storage, API design and portability](docs/38-esp-resident-education-runtime-architecture.md)
- [28 end-to-end acceptance cases, release gates and beginner-to-advanced experience criteria](docs/39-seth-end-to-end-acceptance-and-coverage-gates.md)
- [130-topic curriculum coverage audit](design/education/curriculum-coverage.v1.json): **22 authored seed lessons, 108 planned** (not yet written)
- [22 original versioned lesson definitions, four explanation depths and optional conceptual checks](design/education/lesson-catalog.v1.json)
- [Bounded, heap-free pure-C teaching-state prototype](prototype/esp_education_core.c) and [host regression test](firmware/BrickOS/tests/test_education_core_host.c)

The learner's `understanding`, the **engine's physical-hardware verification**, and the **calibration's evidence/validation** always remain independent states; a correct educational answer cannot authorize an ECU write or certify a vehicle as safe. Lessons and semantic roles are authored once for the ESP-owned packet architecture; a future DS client would render them without ECU-specific hardcoded settings. **No full language model is assumed to run on the 4 MB target**; the intended design is a deterministic rule engine and indexed flash-backed educational library with measured resource limits. A qualified, independent inspection/validation may still be necessary before engine/vehicle operation.

## Verified pre-implementation research (November 19, 2026)

**No runtime implementation in this research pass.** The latest documents identify the actual public firmware/INI rules, current browser alias candidates, unresolved board/ECU evidence, and release gates before Seth's ESP-resident guide is integrated.

- [Verified rusEFI/TunerStudio/HPA/ESP-IDF research ledger](docs/40-verified-external-source-ledger-and-rusefi-constraints.md) — upstream facts versus what requires the matching generated INI or external proof
- [Implementation readiness, 10-stage task ownership, semantics and unresolved hardware decisions](docs/41-implementation-readiness-decisions-and-traceability.md)
- [46 unverified semantic-source candidates](design/research/source-role-candidates.v1.json) — 25 from the current browser code, plus 21 research hints; **none certified against a live Brick ECU**
- [Source-ledger regression test](firmware/BrickOS/tests/seth-role-ledger.test.mjs) — catches accidental divergence from the current browser alias list

**Key findings:** The real rusEFI `engine` channel is a bitmask; generated INI page ordinals and optional pages can vary; display aliases may share one stored calibration value; specialized trigger capture is not ordinary 100 Hz output telemetry; PWM pump control does not imply a missing regulator or verified rail pressure; and the current 4 MB ESP partition plan has no dedicated profile/content storage or OTA slot. The exact live ECU INI, hardware flash/PSRAM, bus throughput and physical build facts remain **unverified** and must be checked before production claims.

**Behavioral constraint:** the full educational guide is separate from hardware/vehicle verification, and the current Bridge/Manual Tuning/3DS connection paths remain untouched. GDI and mixed GDI/PFI Guided Fuel Tuning are unsupported.

## Real generated Proteus F7 INI research (November 19, 2026)

A complete customer-supplied generated INI was analyzed **as a definition, not a live ECU configuration**: signature `rusEFI master.2026.11.09.proteus_f7.2735508644` (SHA-256 `24f696ac500cf48a23d3332a235654442789dac3fe08948656a36da8c320eedd`). It declares five pages, 1,977 typed calibration definitions, 1,081 typed output fields, 116 table editors, 218 submenu entries and 124 command buttons.

- [Detailed generated-INI audit, role corrections, true table geometry and ten-stage guide mapping](docs/42-proteus-f7-generated-ini-deep-audit.md)
- [Exact-source research metadata / selected canonical byte locations](design/ini/proteus-f7-2026-11-09-source-audit.v1.json)
- [Reusable read-only INI research scanner](tools/ini_research_scan.py), with [synthetic parser tests](tools/tests/test_ini_research_scan.py) and [source-fact tests](firmware/BrickOS/tests/proteus-f7-ini-audit.test.mjs)

**Important:** This INI's declared identity has **not** been matched to a live ECU and supplies no actual tune values or physical engine facts. The browser Guide's current alias candidate for spark and low-pressure fuel sensing misses the correct output channels; the existing `sync` and `knock` aliases conflate counters with other quantities. **These are research findings, not runtime fixes.** Present primary/secondary/idle VE shapes are 16×16 / 16×16 / 4×4, and page 3 LTFT is explicitly read-only. GDI and mixed PFI/GDI Guided Fuel Tuning stay unsupported. Manual Tuning, ESP bridge and DS/DSi connection code were not modified.

## Phone INI loading: ECU SD first, fast local cache

**Primary supported definition:** the user-provided generated Proteus F7 INI `rusEFI master.2026.11.09.proteus_f7.2735508644`; unknown future/third-party strategies are outside the verified BrickOS scope until explicitly supported. Matching the connected ECU signature remains mandatory before calibration.

The existing 3DS/DSi tuners preserve a verified copy on **their own SD card**. For the ESP-hosted phone, the existing path is **ECU SD via USB MSC LUN1 → ESP SPIFFS**, now enhanced with a **stable ESP SHA-256 fingerprint → browser IndexedDB parsed-definition cache**. Returning to the same ECU and unchanged ESP INI can reuse the parsed definition without a repeat ~944 KB transfer. The first load still uses the original ESP source; browser storage failure falls back to network; manual upload remains available. Neither DS connection code nor ECU commands were changed.

- [Detailed support boundary, automatic load architecture and validation checklist](docs/43-supported-ini-scope-and-phone-sd-fastpath.md)
- [Phone cache module and exact hash/signature fallback](firmware/BrickOS/web/ini-fast-cache.mjs)
- [Cache regression tests](firmware/BrickOS/tests/ini-fast-cache.test.mjs)

**Host-tested only:** no measured load-time improvement, real phone test, new ESP-IDF binary build, or physical ECU/MSC confirmation has been performed for this update. The new browser asset is regenerated automatically by ESP-IDF CMake configuration.

## Seth standalone prototype V0.2 — no production integration

[Prototype source and build instructions](prototype/seth-agent/README.md) — a self-contained, **offline C11** agent prototype, not linked into `firmware/BrickOS/` or the phone/DS applications.

V0.2 implements stage-by-stage task selection, prerequisite-aware lesson paths, source-independent user-reported engine profile decisions, allowlisted/revisioned dialogue actions, bounded CRC-checked resumable progress records, and synthetic timestamp-aware diagnostic evidence review. The embedded JSON response includes explicit reasons and next tasks, while **all tuning writes and physical engine controls remain absent**. No live telemetry, ECU matching, SD persistence or autocalibration has been enabled.
