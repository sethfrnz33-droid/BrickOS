# BrickOS portable guide: one dialogue source for phone and DS

**Implementation status:** the ESP-hosted browser runs the read-only Seth dialogue in JavaScript and the ESP serves the versioned dialogue JSON at `GET /api/guide/v1/catalog`. The DS/DSi port and ESP-resident semantic resolver are **not yet implemented**; this document specifies how they should work without requiring console app rebuilds for ordinary INI changes.

## Non-negotiable architecture

1. **The matched ECU INI controls what's real.** Resolve firmware signature, pages, table metadata, output channels, visibility and current configuration before showing a readout or a direct editor link. No guessed offsets, fabricated axes, or fictitious capabilities.
2. **Dialogue content lives on the ESP, not in a console binary.** The JSON dialogue catalogue is versioned and stored alongside ESP firmware; clients download/can cache it by its revision. A later ESP configuration/profile store can update the catalogue/rule pack independently of a 3DS release, once signed/validated updates are supported.
3. **Semantic roles are a mapping layer, not hardcoded ECU addresses.** The catalogue contains named roles such as `rpm`, `lambda`, `idleTarget`, `sync` and bank-specific trims. The ESP resolves each role from the **matching** INI. If no binding is supported, emit `not exposed` rather than assigning a guessed channel.
4. **The guide is a state machine, not an ordered list of arbitrary field writes.** Node IDs and choice destinations are stable, explicit, validated, and versioned. Each node is a text/dialogue payload; no UI owns technical logic.
5. **Every calibration field is still reachable.** Full tour covers systems; targeted help covers common symptoms; Configuration Atlas covers the entire decoded INI inventory with dynamically generated per-field help. Unknown semantics are labeled unknown. The main manual editor remains intact.
6. **3DS/DSi/phone render one server-issued view model.** The phone currently interprets the JSON and reads INI-derived browser state. The **future ESP-resident** resolver must issue the same packet over lightweight HTTP, so DS doesn't need to parse an ever-changing INI or embed dialogue text.

## Current browser implementation (what works now)

- `firmware/esp32/web/guide-catalog.json`: all authored Seth dialogue, topic branches, stage IDs, semantic-role alias registry, sprite/audio URLs.
- `guide-story.mjs`: validates node references, drives full/targeted paths, expands `{{available:...}}` from the currently verified browser INI and fresh data, generates dynamic field pages, and forbids links to unknown menu entries.
- `guide-story-ui.mjs`: phone dialogue/card presentation; no decisions or command IDs are embedded in display text.
- `guide-voice.mjs`: user-provided MP3 and independently layered portrait with jaw movement. Text is accessible without audio or animation.
- `app.js`: a separate phone-only story route and fallback navigation to pre-existing evidence/Atlas/manual editors. Existing USB, TunerStudio, 3DS and DSi network protocols are unchanged.

### Shared packet shape (proposed final ESP output)

```json
{
  "schema": "brickos.guide.dialog.v1",
  "catalog_version": "1.0.0",
  "node_id": "issue.idle.surge",
  "speaker": "seth",
  "title": "RPM surges or oscillates",
  "text": "Diagnostic explanation using verified current data, or stating unknown",
  "stage": "idle",
  "definition_key": "ECU signature plus definition fingerprint",
  "freshness": "live | stale | unavailable",
  "choices": [{"id":"issue.idle.surge:0","label":"Show relevant ECU readings","go":"evidence.idle","enabled":true,"blocked_reason":null}],
  "manual_target": null,
  "field_id": null,
  "read_only": true,
  "voice": "/guide/voice.mp3",
  "assets": {"head":"/guide/seth-head.png","jaw":"/guide/seth-jaw.png","mouth":"/guide/seth-mouth.png"}
}
```

This is a shape example, **not a captured ECU response**. The current route serves the static catalogue, not the runtime packet. A later `GET /api/guide/v1/state` and `POST /api/guide/v1/choice` (with authenticated session / replay protection if stateful) should move role resolution and node evaluation to ESP. Do not expose calibration write or burn routes through the dialogue protocol.

## INI changes and clients

- **New firmware signature or changed INI layout:** invalidate stale role bindings, old table locations and saved in-progress state. Device requests a fresh packet and shows a reason for anything unresolved.
- **New field within compatible INI:** Atlas automatically inventories it; generic help shows raw definition/type/units and links to its real menu if present. A new semantic rule may be delivered as an ESP rule-pack update without changing DS software, but new live-control capabilities require separate validation.
- **Hidden menu item or disabled strategy:** maintain inventory metadata but mark context inactive/unknown and do not assume the table is in use.
- **Unrecognized output channel:** do not infer meaning or scaling based only on a similar name. Preserve the channel identity and raw metadata; a verified rule is required to interpret it.
- **Multiple clients:** the phone and DS may browse read-only independently; the ESP owns authoritative diagnostic state when the server-side engine exists.

## Console-port instructions (future, no source edits yet)

Build a small, fixed viewer: fetch the catalogue/packet, display `speaker`, `title`, typed `text`, options by ID, and preloaded mascot mouth/head/jaw assets. Defer imagery/audio updates independent of the ECU sampling loop. Selecting a choice sends only the stable choice ID (never a memory offset); the ESP returns an updated packet. If a new node/card type is unsupported, the console shows plain text and choices, not a crash. Protocol changes require schema negotiation, with a compatible fallback.

Keep 3DS connections, TCP/HTTP/INI logic and their current tuning navigation unchanged until this contract is implemented and hardware tested. No console build is required merely to revise authored Seth dialogue, rename a guide section, or add a verified ECU field role to the ESP pack.

## Controls and safety

Dialogue navigation is **read-only** and should remain usable even with no ECU connected. Do not grant ECU control based on possession of the open Wi-Fi AP. For a future real autotune controller, authorization, heartbeat, exclusive writer, revision-based checks and explicit RAM-vs-flash confirmation are separate and are not implemented by guide dialogue.

## Asset handling and lore

Use the supplied portrait as one fixed sprite coordinate canvas: transparent upper face, separate lower jaw following the real lower-lip seam, and mouth interior drawn underneath. Only jaw moves while talking. Play a portion of the user-supplied nonverbal clip on intentional dialogue navigation; keep audio optional and muted when the user selects it. Technical text remains fully readable. Seth's personality is understated, occasionally rueful about being confined to the ECU, and helpful even when he complains.

## Regression requirements

- Node graph all targets resolve, including dynamic `field.N` and `back`.
- Missing/stale INI roles never masquerade as measured values.
- A changed ECU/INI definition invalidates the prior dialogue binding.
- Every decoded INI field has a generic guide page and *only* a verified existing editor target.
- Phone and future DS receive equivalent IDs, choices, source revision and caveats.
- Audio autoplay rejection or missing sprite does not block guide navigation.
- Existing Manual Tune, TunerStudio bridge, 3DS/DSi software and write paths have no changes from mascot or guide code.

## v1.2.0 chapter question packets

The shared versioned catalogue now includes `chapter_checks` keyed by the ten HPA-ordered chapters. Its interpreter returns dynamic `check.<chapter>.<index>` dialogue nodes and accepts `answer.<chapter>.<index>.<reported|unknown>` review notes, plus `packet.check` and `packet.chapter_progress`. A report is **never** equivalent to sensor or physical verification. The phone currently stores notes only in its own browser, scoped to matched ECU signature plus INI fingerprint and catalogue version. Future DS/DSi portability requires moving this interpretation and cross-device state ownership onto the ESP, without embedding English dialogue or raw INI offsets in the console binary. [All 30 authored questions](28-seth-chapter-questions.md).
