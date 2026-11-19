# Brick Tuner Suite 3DS: Guided Tuning screen plans

**Design only; no 3DS UI code changed.** Work from the imported `apps/3ds/` v6.3 architecture. The existing home, TunerStudio-category tree, `View` grouping, map/curve/scalar editors, graphs, gauge mode, favorites and manual controls stay intact. Do **not** create duplicate global ECU maps, duplicate live channels, or an alternative manual tuning tree.

## Physical rendering contract
Top: nominal **400x240 per eye** of 3D-capable top display (800x240 stereoscopic transport). Bottom: **320x240** touch display. Treat the upper display as read-mostly and the lower display as the interaction surface. On the top, use readable 1-bit/2-bit status symbols with clear labels and no text smaller than the existing readable font scale. 3D stereo is optional decoration, **never** part of safety-critical text or state.

A Guided Tuning entry belongs on the existing home launcher as a **distinct workspace**, not as a replacement for `Tune`. Existing Tune should continue to open the unmodified INI-derived TunerStudio navigation. Optionally display a small `Guide findings` count on home, never an intrusive modal.

## Navigation architecture
Guided mode has `NEXT | SYSTEMS | EVIDENCE | ATLAS | HISTORY` and context pages. The navigation state on 3DS is independent of the phone: the device asks the ESP for current state and paginated content, and resumes **read-only** after reconnect. The ESP is the source of truth for findings and session state.

Proposed button mapping (must be checked against existing modal/editor mappings before implementation):
- **A:** open highlighted finding/field or confirm a *navigation* choice.
- **B:** return to prior guided page; from guided home return to normal home; never implicitly save/burn.
- **D-pad / Circle Pad:** move between cards/rows; while a heatmap is focused, navigate the selected region.
- **L/R:** previous/next guided workspace or relevant subpage; not a hidden tuning control.
- **X:** `Why?` explanation/evidence for the highlighted finding.
- **Y:** toggle summary/details where it avoids crowding.
- **SELECT:** return to normal home only after explicit warning if a session is active; stopping and leaving are not synonyms.
- **Touch:** large rows, breadcrumb/back, scroll, single-tap focus; activation/confirmation is explicit. No destructive action relies on a tiny touch target.
- **START:** session menu, showing monitor/learn state and requiring deliberate action. Exact bindings remain subject to source audit.

## Screen A: Guided home / next action

Upper 400x240 (conceptual, not a raster mockup):
```
+--------------------------------------------------+
| BRICK TUNER SUITE       ECU: VERIFIED  WiFi: OK  |
| GUIDED TUNING                        OBSERVING    |
|--------------------------------------------------|
| NEXT PRIORITY  1 / 4                             |
| Idle airflow: control authority needs review     |
|                                                  |
| Evidence: RPM target misses; actuator data weak  |
| Confidence: limited   Data: current              |
|                                                  |
| Why it matters: dependent idle assessments wait  |
| [What changed?]      [Open relevant setting]     |
+--------------------------------------------------+
```
Lower 320x240:
```
+----------------------------------------+
| NEXT  SYSTEMS  EVIDENCE  ATLAS  HISTORY |
|----------------------------------------|
| > Review idle actuator feedback        |
|   Check lambda sensor validity         |
|   Inspect startup diary                |
|----------------------------------------|
| [WHY?]  [EVIDENCE]  [MANUAL SETTING]   |
| Status: READ ONLY       B: Back        |
+----------------------------------------+
```
Top **states the question and evidence**; bottom provides selectable next actions and links. Long explanations scroll on lower screen. No pop-ups during normal manual tuning.

## Screen B: Lifecycle map / systems
Top shows 6–8 compact stages, each labeled `verified / blocked / needs evidence / inactive` (not a fictitious `% tuned`); lower lists the selected stage's dependent subsystems:
```
TOP:  Identity [OK] -> Sensors [!] -> Start [--]
      Idle [WAIT] -> Fuel [WAIT] -> Spark [REVIEW]
      Progress: 34 / 52 applicable fields classified
BOTTOM:  Idle
         > Airflow [needs verification]
           Fuel [waiting for lambda]
           Spark [read-only evidence]
           Controller [waiting for airflow]
         [WHY BLOCKED] [OPEN IN TUNE]
```
A stage may be browsed before prerequisites are complete, but data-dependent actions remain visibly blocked with reasons.

## Screen C: Idle and starting review
Top can render a single large, readable trend: actual/target RPM and actuator request, or temperature-bucket start-event timeline. Lower provides tabs `AIR | FUEL | SPARK | FEEDBACK | STARTS`, filter-rejection counts and **one** next verification task. Do not make multiple high-frequency graphs fight for top-screen rendering.

## Screen D: VE learning
Top renders a **read-only evidence heatmap** of the *active INI table*, showing region coverage or candidate delta overlay, not an editable duplicate of the manual map. Show label and numeric value for focused region, legend for `not seen / invalid / observed / sufficient`, bank and source strategy, and ECU revision.
Lower:
```
+----------------------------------------+
| VE LEARNING       MODE: OBSERVE         |
|----------------------------------------|
| Eligible: --   Excluded: --            |
| Active filter: Lambda not ready        |
| Focus: RPM band / load band            |
| Accepted visits: --  Confidence: --    |
|----------------------------------------|
| [FILTERS] [WHY REJECTED] [PROPOSALS]   |
| [OPEN ORIGINAL VE EDITOR]  B: Back     |
+----------------------------------------+
```
Filters appear as concise selectors; advanced settings are an optional paginated dialog, not permanent clutter. Any future RAM action has a separate transaction review page and never occurs from heatmap navigation.

## Screen E: Evidence / Atlas / history
- **Evidence:** top waveform or plot with timestamp and measurement source; lower channel freshness, exclusions, and timeline list. Do not duplicate the original `View` dashboard; this is **evidence about a finding**, not a second live-gauge workspace.
- **Atlas:** top field path, type, active strategy and concise description from INI or verified rule; lower INI-native menu/folder navigation and `Open in Manual Tune`. Unmapped fields remain visible as `not yet understood`. No implicit table creation.
- **History:** top key event and revision comparison; lower dated event list, source and fault reason. Allows replay without touching ECU.

## Screen F: Review a proposed change (future, disabled initially)
Top summarizes `what / why / original / proposed / touched cells / expected evidence` and identifies uncertainty. Bottom shows the explicit choices `VIEW ORIGINAL | DISCARD PROPOSAL | [APPLY DISABLED UNTIL VALIDATED]`. No auto save. No auto ignition advance. If ECU revision changes, show `Proposal stale` and remove apply availability.

## Device session controls
Session menu displays owner/pairing, mode, heartbeat freshness, ECU state, and the difference between `pause learning`, `end session`, `discard proposals` and `save calibration`. Later write-capable builds must require a separate transaction-specific confirmation; physical buttons and touch shortcuts cannot bypass it.

## Readability/latency acceptance criteria
1. Every finding is reachable in at most a few predictable navigation steps from Guided home.
2. Inactive/blocked controls explain why and point to the exact configuration item.
3. Existing normal tuning, gauges, logged data and INI-derived View tab remain unchanged.
4. No repeated title/header on both screens unless context needs it; upper display visualizes, lower display navigates.
5. Every selected field has a fallback title/description when INI text is missing; never show a misleading editable control for an unsupported type.
6. Offline/disconnected data is prominently tagged stale and never labeled live.
7. Paging and compressed status updates keep Wi-Fi redraw independent of ECU acquisition.
8. Device unpair/reconnect cannot resume a prior write authority.
9. Stereo and gyro effects may enhance decoration only; diagnostic text retains readability with stereo turned off.

## References
Existing repo layout: `apps/3ds/RELEASE_V37_TUNERSTUDIO_TWO_SCREEN_TREE.md` (focus transfer and INI menu), `apps/3ds/RELEASE_V63_3D_MAP_CAMERA_FIX.md` (current imported release), `apps/3ds/README.md`.
Hardware screen reference: https://www.3dbrew.org/wiki/Hardware
