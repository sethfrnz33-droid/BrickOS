# BrickOS phone browser: Guided Tuning responsive UX plan

**Design only; no web UI files changed.** The imported `firmware/esp32/web/` app remains the normal browser-based tuner; Guided Tuning is a separate opt-in workspace, not a replacement for Manual Tuning or its INI-driven navigation. Both 3DS and phone consume the same ESP guided API; **no duplicate reasoning engine runs on the phone**.

## Navigation contract
Persistent global mode switch: `Manual | Guided`; existing Manual view must remain exactly as it is. Guided has five destinations: `Next`, `Systems`, `Evidence`, `Atlas`, `History`. On portrait, use labeled bottom navigation; on landscape/tablet a compact side rail is allowed. Avoid a separate global Maps/Gauges browser that duplicates the existing editor.

The header is always visible:
`BRICKOS   ECU <identity>   <live/stale/disconnected>   <Observe/Learn/Paused>`.
Connection/identity state and current operation are textual, not color-only. Pressing it opens session, pairing, and data-freshness details.

## Phone portrait: main Guided dashboard
Illustrative structure, designed to reflow from narrow mobile widths rather than assume one phone resolution:
```
┌───────────────────────────────────┐
│ BRICKOS  [Guided ▾]   ECU ● LIVE  │
│ Session: Observe     Details     │
├───────────────────────────────────┤
│ NEXT ACTION                       │
│ Verify idle actuator response     │
│ Why: RPM misses target; command  │
│ evidence is incomplete            │
│ [WHY?]         [VIEW EVIDENCE]    │
├───────────────────────────────────┤
│ ENGINE SYSTEMS                    │
│ Identity              Verified   │
│ Sensors               Review     │
│ Starting              Observing  │
│ Idle                  Blocked    │
│ Fuel                  Not tested │
│ Ignition              Read-only  │
│ [ALL SYSTEMS]                     │
├───────────────────────────────────┤
│ CURRENT EVIDENCE                  │
│ No eligible fuel samples         │
│ Reason: lambda not verified      │
│ [SEE EXCLUDED SAMPLES]            │
├───────────────────────────────────┤
│ Next | Systems | Evidence | Atlas│
│          History                  │
└───────────────────────────────────┘
```
Avoid putting all five destinations in tiny illegible tabs; adapt as a scrollable bottom bar or a labeled `More` slot at narrow widths. Never imply an illustrative reading is real.

## Page 1 — Next
One dominant task with `What we saw`, `Why this matters`, `Other possible causes`, `Evidence still missing`, and `Open this setting in Manual`. Show a small queue of two additional issues, not a hundred warnings. A `Not now` dismissal changes presentation priority but **does not mark a blocked safety dependency verified**.

## Page 2 — Systems / full lifecycle
Vertical stepper from ECU identity through sensors, starting, idle, thermal behavior, base fuel, corrections, transients, ignition review, advanced strategies and professional validation. Stages reveal 3–8 category experts rather than a huge flat list. Each stage has status, missing evidence, relevant INI fields, dependency and event count; `reviewed` is not `safe/perfect`. An always-available `Browse all stages` supports knowledgeable users without bypassing gating.

## Page 3 — VE evidence
A vertically stacked, **read-only** heatmap with bank/strategy switch and tabs `COVERAGE | ERRORS | PROPOSALS`; one finger can inspect a cell/region to show sample quality and rejection reasons. Landscape can place heatmap and focused-cell details side by side. A link goes to the **original** table editor in Manual mode, with a breadcrumb back to the guide. No heatmap tap directly edits the ECU.

```
┌───────────────────────────────────┐
│ VE LEARNING    [OBSERVE]          │
│ Strategy: matched INI definition  │
│ [Coverage] [Errors] [Proposals]   │
│ ┌───────────────────────────────┐ │
│ │ Active table evidence matrix  │ │
│ │  unseen / rejected / observed │ │
│ │  no safety claim              │ │
│ └───────────────────────────────┘ │
│ Active filter: Lambda unavailable │
│ Sources: lambda · trims · target  │
│ [WHY REJECTED] [FILTER SETTINGS]  │
│ [OPEN ORIGINAL TABLE]             │
└───────────────────────────────────┘
```

## Page 4 — Idle and start diary
The Idle expert uses horizontal in-page categories `AIR | FUEL | SPARK | FEEDBACK | TEMP`; each opens a compact explanation and an optionally expanded synchronized trace. The start diary uses cards grouped by initial temperature and configuration revision; tapping shows crank/catch/after-start/warmup boundaries with sensor readiness labels. **Do not present a cold-start suggestion from a single event.**

## Page 5 — Sensor / trim detective
Two bank tiles with explicit native units, neutral convention, interpretation and channel availability. Present `base fuel uncertain because closed-loop is compensating` instead of `AFR perfect`. If an ECU version does not expose bank-specific trims, show unavailable instead of inferred zero. The diagnostic view is separate from the existing live channel/gauge view.

## Page 6 — Smart Spark
Shows current ignition strategy, known corrections, reported knock/temperature conditions, and which instrumentation is missing for torque-based conclusions. Labels are `review`, `insufficient evidence`, `qualified external validation needed`. No auto-advance button and no promise to find MBT from MAP, airflow or phone motion sensors.

## Page 7 — Configuration Atlas
INI-native hierarchy filtered by `active`, `critical`, `unknown`, `needs verification`, `recently changed`, and plain text search. Each field detail shows **source name, type, scaling, unit, active condition, description (when provided), related fields, owning expert, revision and missing evidence**. `Open in Manual` performs a direct route to the original dialog; on return, preserve guided finding state. No fabricated explanations for unknown fields.

## Page 8 — History and review
Event cards for start, warmup, sensor fault, manual edit, calibration snapshot and learning session. Proposal comparisons show original versus suggested values, touched regions, source evidence, applicability and uncertainty. `Save/Burn` is never merged into `Accept`; future authorized RAM application must be an entirely separate high-friction confirmation flow with exclusive writer and revision check.

## Session control drawer
Visible, explicit actions:
- `Monitor only`: read-only; no active learning lease.
- `Start guided learning`: authenticated and supervised; obtains a lease and shows expiry/heartbeat health.
- `Pause learning`: stops collecting new eligible learning evidence, retains history.
- `Stop session`: terminates controller lease; no implicit save or reset.
- `Discard suggestions`: discards proposal objects, not arbitrary ECU RAM changes.
- `Open Manual Tuning`: navigates to the pre-existing editor; never strips the user of the original controls.

**Future write mode disabled in initial design.** A site reachable through the current open AP must not grant control authority. Authenticated operation needs separate pairing/transport hardening.

## Responsive behavior and accessibility
- Portrait: one-column tasks and scrollable panels; landscape/tablet: 2-column inspector with persistent status.
- Touch targets comfortably sized and separated; legible font, contrast, reduced-motion preference, no color-only health statuses.
- A primary `Next task` should be reachable immediately when Guided opens; secondary features are still clearly visible by name, not hidden inside anonymous icons.
- Never show gauges in the Guided header unless relevant to a specific active finding; raw telemetry belongs to existing Manual/View/Logs.
- Cache only read-only display snapshots with `stale since` labels. Do not treat phone display state as ECU state.
- Long logs and full INI listings are paginated. A phone reconnect gets read-only history and **must explicitly reauthorize learning**.

## Shared device sync
Each client receives `session_id`, `revision`, `captured_at`, `fresh_until`, `mode`, `primary_finding_id`, `event_cursor`, `alarm_severity` and `source_provenance`. Clients may request updates at their own pace and render independently; the ESP does not perform more ECU reads just because multiple screens are connected. Only one device can hold an active controller lease; other viewers remain read-only.

## Layout acceptance criteria
1. Switching Guided/Manual never changes manual ECU calibration or INI navigation.
2. Three taps or fewer from Guided home to primary recommendation, relevant evidence, and original manual editor.
3. Missing sensor or communication state displayed as `unknown/stale`, never normal.
4. All restrictions and blocked dependencies have visible, comprehensible reasons.
5. Same finding ID, explanation and source revision appears on both phone and 3DS.
6. UI redraw and network interruptions do not pause critical ECU acquisition.
7. Leaving the page does not implicitly burn/save; after lease timeout no further automatic ECU actions can be authorized.
8. Guided interface never suggests operating a phone while driving; use stationary or otherwise professionally supervised setups.

## References
Current web app: `firmware/esp32/web/`; existing Guided UX rules: `docs/10-user-experience.md`; authorization and review design: `docs/18-transaction-and-review.md`.
