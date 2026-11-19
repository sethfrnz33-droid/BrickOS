# ESP32-S3 Runtime Engineering Plan for Seth's Guide

> **Engineering design and partially host-tested C prototype — not ESP-IDF integration.** The current firmware's phone browser executes Seth's story and diagnosis; the new `prototype/esp_education_core.c` is heap-free, with host tests, but isn't in `firmware/esp32/main/CMakeLists.txt`. Before a "runs on ESP" claim, integrate, cross-compile, inspect map/heap, and run real target hardware tests.

## 1. Target reality (source-derived)

| Source | Confirmed repository configuration |
| --- | --- |
| `firmware/esp32/sdkconfig.defaults` | ESP32-S3, ESP-IDF project defaults, **4 MB flash configured** |
| `firmware/esp32/partitions.csv` | 24 KiB NVS `0x6000`; 2.5 MiB factory app `0x280000`; ~1.44 MiB SPIFFS logs `0x170000` |
| `firmware/esp32/main/telemetry.c` | Shared USB output-block polling, 10/20/50/100 Hz **requested**, actual rate reported separately; host's interest has a timeout |
| `firmware/esp32/main/web_assets.c` | Source-generated static browser assets, mascot portrait and voice embedded at compile time |
| `firmware/esp32/main/CMakeLists.txt` | Explicit firmware source list; education prototype **not presently part of it** |
| `firmware/esp32/web/guide-story.mjs` | Current dialogue and self-report state run client-side, tied to an INI/ECU key |
| `prototype/esp_education_core.c` | New small, fixed-array C education state, independent of the ECU, currently host-tested |

All values above describe **partition allocations/configuration**, not verified available binary space or RAM. Check actual board flash/PSRAM, build map, free heap, stack watermarks, storage free space and transport constraints. Source text size is not binary size. Current partition plan reserves **no OTA slot and no dedicated education/profile partition**. Do not assume external SD, onboard NAND, PSRAM or unused log storage.

### Strongest design choice

Avoid storing one giant active JSON object, raw INI, all logs, and every lesson in RAM simultaneously. No GPT-like generative language model on this 4 MB design is assumed or promised; Seth's apparent intelligence comes from **structured evidence reasoning + adaptive lesson planning + a carefully written, versioned text catalog**.

## 2. Proposed ESP components and ownership

```
  ECU native USB / source-specific diagnostics
     | (existing USB scheduler, highest authority)
  Shared telemetry snapshots / bounded events ----> fault & validity state
     |                                                     |
  INI signature / active config resolver ---> semantic role registry
     |                                                     |
  Engine archive + learner profile (separate versions / opt-in)
     |                                                     |
  Seth Coordinator (FSM + expert graph + lesson prerequisites)
     |                                                     |
  Lesson Catalog Reader -> Dialogue Packet Composer -> Paged API
                                   |             |
                              phone browser   later DS
```

Services:
- `definition_registry`: only matches **verified** ECU signature and valid INI page schema; maps role IDs to explicit field paths and units; supports conditional active strategies and unknown result.
- `engine_archive`: physical build claims, proof categories, source metadata and profile revision; local engine ID independent of firmware. Authenticated editing required.
- `learner_state`: chapter/concept progress and presentation preferences; never shares a verification flag with engine archive.
- `lesson_reader`: fetches a bounded lesson by ID and content version; returns short/long/technical variant and next optional check; no sensor reads inside the UI worker.
- `case_agent`: reads immutable evidence and relevant profile facts, creates a finding with contradictions and missing input; only vetted semantic rules.
- `course_coordinator`: chooses next chapter/concept/task by prerequisites, operating state and user's selected mode; respects `not now` and user's current intent.
- `packet_builder`: single source of words, choice IDs, evidence and manual deep-links for all displays.
- `session_supervisor`: profile selections, controller role, heartbeat and reset policy; **does not** grant ECU control from tutorial actions.

## 3. Minimal task scheduling

Give existing ECU communication and fault-handling paths priority over logging, browser updates and story generation. **Do not rewrite the ECU's real-time engine loop**; it remains on the STM32. Example task split is conceptual—actual FreeRTOS priorities and core affinity require measured profiling.

1. ECU transport arbiter (existing: never block on Seth, phone or audio).
2. Bounded data acquisition and fault preservation (read-only consumer).
3. Optional event recorder with nonblocking ring and overrun counters; no unbounded writes.
4. Low-priority semantic reasoner (throttled on case change or new qualified evidence).
5. Very-low-priority lesson/task coordinator (on user action, new verified state or diagnostic milestone; **not every ECU frame**).
6. HTTP API server sends compact, paginated and revisioned text/plot descriptors.
7. Browser/3DS animation, voice and rendering occur entirely on the device; the ESP never renders bitmaps for each frame.

When overloaded, **drop display updates and optional samples** before delaying USB/protocol acknowledgements. A lengthy lesson string cannot hold a telemetry mutex. No lesson callback may reenter a calibration write function.

## 4. Fixed-budget memory approach

The current pure-C prototype deliberately uses no allocation; `brick_edu_state_t` has a fixed record array of 48 concepts, 48-byte IDs, depth/interaction fields and a short state per concept. The bundled lesson catalog currently has **22 concepts**. The C state is **not** a storage/persistence engine; it only shows a feasible bounded state representation.

Target design constraints (to **measure, not advertise as achieved**):
- Each API reply has an explicit maximum number of choices and byte size. Reject oversize text, malformed UTF-8, giant arrays and recursive references.
- Read **one lesson node or short page** at a time; avoid parsing the full ~45 KB design catalog into heap every request.
- Compile a generated compact index from validated JSON at build time, with offsets into read-only flash bytes; optionally compress repetitive static strings where a measured flash win exceeds decode CPU cost.
- Retain only current dialogue state, current concept set, and bounded case-summary references in RAM; detailed waveform/log storage uses verified media and backpressure.
- Keep durable profile learning/checkpoint data separate from high-churn event streams and from ECU configuration/calibration stores.
- Record retained evidence/cases with exact schema version, checksums and commit status; recover power loss without treating a partial record as valid.

Before release produce a build report with actual `idf.py size-components`, `idf.py size-files`, binary fit against `factory`, `heap_caps_get_free_size`, `heap_caps_get_largest_free_block`, task stack high-water marks, allocation delta after 1,000 lesson hops, log partition occupancy and typical/max endpoint response latency. No assumed PSRAM for baseline.

## 5. Storage partitions and durability

NVS is appropriate for **small preference and checkpoint records** subject to wear/size review; it is not an unlimited log store. The existing `logs` SPIFFS partition belongs to the firmware logger and **cannot be casually repurposed** for profile archives. Make a separate storage plan with capacity math, retention limits, atomic write strategy and explicit upgrade/migration before adding persistence. Prefer strict retention and export over deleting user evidence silently.

Content packaging options in order:
1. **Initial:** catalog compiled into const flash indexed by concept ID and version. Changes require ESP firmware update but **no phone/DS update**. The matched INI changes field bindings without rebuilding lessons.
2. **Later:** verified, signed and versioned external content bundle on explicitly provisioned partition/media. Reject mismatched schema and return to a built-in minimal guide.
3. **Optional:** authenticated archive export/import with version migrations, never automatic mass overwrite of hardware facts.

If content doesn't fit the production flash partition, **reduce assets and separate optional lesson bundles**; do not erase logs or change partition layout without migration and explicit device/flash-size verification.

## 6. Explicit knowledge-packet interface

Proposed responses, **not deployed**:

`GET /api/seth/v1/capabilities` -> `schema,` build ID, active ECU definition hash, supported lessons, recording/diagnostic abilities and unsupported reason codes.
`GET /api/seth/v1/catalog/index?cursor=...` -> list of concept IDs, chapter, prerequisites and content revisions.
`GET /api/seth/v1/lesson/<concept>?depth=guided` -> `dialogue_id`, text, optional diagram/sources, a question and source IDs, blocked reasons and allowed **non-actuator** actions.
`GET /api/seth/v1/session` -> learner depth/interaction preference, selected engine project, current lesson/chapter/case, prompt state, latest actual MCU evidence freshness.
`POST /api/seth/v1/session/action` -> `action_id`, expected lesson/learner revision, idempotency key; deterministic return packet, no embedded TS write command.
`GET /api/seth/v1/evidence?case=...` -> paged trace, diagnostic hypothesis and data validity; *no overload of a lesson packet with unbounded raw samples*.
`GET /api/seth/v1/archive` and eventual `POST` -> separately authenticated opt-in project/profile archive operations only after threat-model design.
`GET /api/seth/v1/course/report` -> independent learner, hardware, calibration observation and external validation statuses.

Payload fields: `schema_version`, `content_hash`, `concept_id`, `source_definition_hash`, `user/session revision`, `current_mode`, `speaker:text`, `choices[]`, `missing_semantic_roles[]`, `facts[]`, `verified/provisional source flags`, and `read_only=true`. An offline/earlier console that doesn't understand an optional diagram falls back to text.

**No autonomous write endpoints** in this educational service. An eventual read-only case consumer cannot acquire a control token through `POST /session/action`.

## 7. INI-driven guide without updating each DS

- Runtime matches ECU signature and INI/layout.
- Rule registry maps **semantic roles** such as `idleTarget`, `clt`, `lambda`, `gego` to validated fields and conditions, or returns `unavailable`. Do not choose the first regex label match as a permanent field identity.
- Lesson catalog references **semantic roles and concept IDs only**, never hardcoded page/offset/pin/physical wiring facts.
- The ESP composes the same text/screen/choice packet for phone and future DS.
- A changed INI updates only resolved ECU fields; it may invalidate related evidence/cases. Learning about basic concepts and physical engine-profile facts persists.
- A genuinely new ECU strategy or renamed/changed semantics may require a verified **ESP rule-pack update**. `INI dependent` does **not** imply all unknown engine logic is automatically understood.

## 8. Learning session state and failure recovery

States: `WELCOME -> ONBOARD -> CHOOSE_PATH -> TEACH -> OPTIONAL_CHECK -> HELP_SUBLESSON -> RETURN -> SUMMARY`, with independent `PAUSED / OFFLINE / STALE / STORAGE_ERROR` flags. A technical or simple explanation is a render choice, not a new engine-control state.

A `concept_result` changes **only learner state**, never `hardware_verified`. Source claims from the physical archive have separate trust/provenance. Lessons never increment a `finished tune` counter.

On ESP reboot: invalidate any volatile control session; recover only completed, checksummed read-only lesson/profile records. If a stored lesson version differs, show `Review available` for affected concepts rather than wiping the archive. A phone disconnect during read-only teaching may resume the same concept if a valid checkpoint exists.

## 9. Deterministic input interpretation

The user's free-text problem description is matched to a verified intent taxonomy, negation/qualifier-aware keyword bundles, one or more scenario IDs and the current engine context. This can be implemented as a small DFA/trie or tagged token matcher on ESP; it is **not** a general-purpose natural-language model.

Rules must support:
- `It won't start` versus `it starts then stalls` (negation and timing).
- `The idle is wandering` versus `idle is good, the sensor wanders`.
- `I have a PWM pump` stores pump *control*, never regulatory architecture.
- `Do I need to learn what AFR is?` enters lesson, not a tune-edit operation.
- Unknown prompts produce `I haven't understood the symptom; pick the closest category` instead of guessing.
- User text is **untrusted** and never interpolated into an ECU command, URL, file path or rule expression.

## 10. Watchdog and acceptance strategy

**Unit:** catalog/schema/prerequisite/unknown role tests; educational correctness and `I don't get it`; fixed-buffer overflow, invalid IDs and arbitrary preference changes; profile/INI version isolation.

**Stress:** one thousand lesson actions, multiple simultaneous phone readers, future DS requests, live 100 Hz requested ECU gauge stream and high-rate recorded event; zero silent corruption, buffer overflows or unbounded allocations. Measure actual delivered sample rate; no requirement that it equal the configured target.

**Fault injection:** storage full, Wi-Fi dropout, ESP reboot, malformed packet, oversized INI, lost sensor, disabled ECU strategy, stale snapshot, unknown firmware signature, firmware definition mismatch, broken lesson-content reference and content rollback attempt.

**Integration:** compile into production ESP32-S3 with the actual 4MB partition; confirm binary fit, heap margin, stack watermarks, watchdog stability and real USB/ECU coexistence. Test without PSRAM; if target boards have verified PSRAM, treat it as an optional enhancement.

**Safety:** a learner answering correctly, choosing `just recommend`, or losing Wi-Fi must never emit a calibration write/burn, engine-start or physical actuator command. Existing manual tuning and 3DS/DSi transport remain separate and unchanged.

## Current status vs target

| Piece | State |
| --- | --- |
| 22 curated original lesson concepts and 10-chapter mapping | **Design data** |
| Bounded heap-free C education state + host tests | **Prototype, host-tested**, not integrated |
| ESP-side lesson bundle generator/reader | **Not implemented** |
| ESP-owned per-learner archive/progress | **Not implemented** |
| ESP-owned INI semantic resolver/expert coordinator | **Not implemented** |
| Full phone educational UI | **Not implemented**, prior Seth story exists |
| DS portable lesson/case renderer | **Not implemented** |
| ECU-side tune changes from guide | **Not implemented and not authorized** |

Sources: current repository partition/telemetry/build files, Espressif ESP-IDF ESP32-S3 partition documentation https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32s3/api-guides/partition-tables.html ; NVS concepts https://docs.espressif.com/projects/esp-idf/en/v5.3/esp32s3/api-reference/storage/nvs_flash.html.
