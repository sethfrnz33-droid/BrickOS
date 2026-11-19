# Guided Tuning feature catalog — observable user behavior

Every item here is **proposed** unless explicitly described as existing in `00-current-status.md`. Features are grouped by which system owns their logic rather than by navigation pages. Many are read-only and useful before any automatic calibration write exists.

## Definition intelligence

| Feature | Exact behavior | Evidence or gate |
| --- | --- | --- |
| ECU Identity Passport | Show firmware signature, board identity, INI/layout hash, page compatibility and last verified revision | ECU reply + matched definition |
| INI Inventory Audit | Count all decoded fields and unresolved raw directives | Parser coverage reconciliation |
| Hidden Field Inspector | Show hidden fields with conditions, never assume hidden means disabled | INI conditional metadata |
| Active Strategy Explorer | Explain why speed-density, Alpha-N or other supported active strategy is selected | Actual calibration flags |
| Duplicate Label Resolver | Distinguish similarly named fields by page and structural identity | Raw page + offset/index |
| Configuration Atlas | Full navigable tree with manual-editor deep links | Exact INI field mapping |
| Field Usage Trace | List which expert, graph edge and live evidence refers to a field | Semantic registry |
| Multi-Table Resolver | Distinguish primary, secondary, idle and blend variants | Verified active strategy |
| Unit and Scale Viewer | Display raw representation and engineering unit conversion | Verified INI decode |
| Unknown Feature Inbox | Group fields requiring semantic engineering | Audit's `NO_VERIFIED_SEMANTICS` list |
| Firmware Update Review | Compare inventory and changed meaning across definition revisions | Both trusted definition snapshots |
| Configuration Dependency Viewer | Show why an option is relevant or bypassed | Graph edges + predicates |

## Engine commissioning and diagnostics

| Feature | Exact behavior | Evidence or gate |
| --- | --- | --- |
| Preflight Review | Explain missing verified ECU/hardware prerequisites | Relevant expert gates |
| Sensor Plausibility Board | Separate physical plausibility from connection and decoding health | Live sensor values + known reference |
| Sync Fault Timeline | Timestamp ECU-reported synchronization events | Firmware channel supported |
| First-Start Diary | Segment events into crank/catch/after-start/warmup | Engine state with timestamp quality |
| Temperature Coverage Map | Show which startup temperature regions have observed events | Validated temperature readings |
| After-Start Recovery Timeline | Compare event timing across different starts | Event classifier |
| Cold-Sensor Readiness Guard | Exclude unready wideband evidence | Lambda validity state |
| Battery/Power Context | Correlate supply behavior with start/idle events without assuming cause | Battery channel |
| Hardware-vs-Tune Diagnostic | Rank mechanical/electrical explanations against configuration explanations | Competing hypotheses |
| Persistent Regression Notice | Reopen a past finding when comparable evidence degrades | Source-compatible repeated observations |
| Stage Pause/Resume | Resume a blocked task after its prerequisite is independently verified | Dependency graph |
| External Verification Tasks | Document that qualified physical checks are required and remain unverified | Human record, not ECU inference |

## Idle and thermal behavior

| Feature | Exact behavior | Evidence or gate |
| --- | --- | --- |
| Idle Strategy Detector | Determine active valve/DBW/open-loop/closed-loop behavior | Configuration and ECU states |
| Idle Air Authority Monitor | Identify apparent saturation or lack of control range | Command, status, target/actual RPM |
| RPM Oscillation Detector | Detect repeatable oscillation windows | Timestamped RPM |
| Lead/Lag Analyzer | Distinguish command-driven oscillation from reactive correction | Timestamped actuator and RPM |
| Idle Fuel Evidence | Associate lambda/trims with active idle fuel strategy | Resolved idle VE/main VE |
| Idle Spark Attribution | Show reported base/actual spark and active corrections | Available ECU channels |
| Thermal Transition Report | Map idle performance across coolant ranges | Multiple independent events |
| Idle Load-Disturbance Journal | Correlate normal observed load-state changes with control response | Verified load context |
| PID Evidence Explainer | Explain why closed-loop control is or is not a plausible cause | Idle state and temporal data |
| Control-Mode Ambiguity Guard | Refuse PID conclusions if idle mode cannot be resolved | Firmware-specific state |

## Fuel model and trims

| Feature | Exact behavior | Evidence or gate |
| --- | --- | --- |
| Bank Trim Separation | Preserve per-bank STFT/LTFT source, scale and freshness | Exact INI and ECU reports |
| `Gego` Interpreter | Keep legacy correction distinct from modern per-bank trims | Per-firmware semantic rule |
| Trim Masking Notice | Explain target lambda may be held by active closed-loop correction | Lambda + feedback state |
| Fuel Pressure Context | Describe pressure source and reference without comparing unlike units | Configured pressure reference |
| VE Influence Visualizer | Show which cells a sample interpolates between | Verified ECU interpolation model |
| VE Region Coverage | Track independent valid visits versus repeated identical samples | Event-level coverage |
| Fuel Sensor Delay Ledger | Store calibration/uncertainty of measurement delay | Valid source/timing metadata |
| Invalid Measurement Explanation | Expose exact reject reason per sample | Per-expert filter |
| Closed/Open Loop Segmentation | Label observations according to fuel feedback state | Verified ECU state |
| Transient Event Gallery | Categorize enrichment, cut and recovery independently | Event flags and timestamped channels |
| Calibration Proposal Preview | Show original/offline-proposed/unknown portions without touching ECU | Versioned read-only model |
| Evidence Replay | Deterministically reproduce any learning conclusion | Versioned samples and rules |

## Ignition, motion and external reference

| Feature | Exact behavior | Evidence or gate |
| --- | --- | --- |
| Actual Spark Audit | Differentiate base strategy and observed spark when channels exist | INI-resolved channels |
| Correction Trace | Identify known correction inputs and explicitly unknown contributors | Verified firmware model |
| Knock Event Journal | Record ECU-reported intervention; no inference of knock-free safety | Knock flags + timestamps |
| Timing Reference Gate | Mark absence of independently verified physical timing reference | Qualified external verification |
| Engine Geometry Record | Store engine facts with provenance and uncertainty | Human/instrumented source |
| IMU Quality Card | Mount, orientation, gravity-compensation, freshness and packet-order checks | Qualified motion source |
| VSS Agreement Summary | Contrast qualified IMU vs speed-derived motion, showing uncertainty | Aligned measurement evidence |
| Torque-Proof Boundary | Explicitly refuse MBT conclusions from indirect signals alone | Tier-1/2 evidence limitation |
| Dyno Import | Read timestamped external torque/power data with instrument metadata | Independent equipment |
| Cross-Session Comparability | Detect confounded comparisons, reject invalid ones | Controlled metadata |
| Spark Hold Advisory | Prefer hold/investigate when evidence does not support adjustment | Risk policy |

## Trust, workflows and usability

| Feature | Exact behavior | Evidence or gate |
| --- | --- | --- |
| Next Best Question | Show the highest-priority *resolvable* missing information | Task ranking engine |
| Why/Why Not Explainer | Expose evidence and competing explanations | Finding trace |
| Deep Link to Manual | Open existing editor at exact referenced field | INI UI mapping |
| No-Hijack UX | Guided mode cannot change manual mode layout or navigation | UI regression testing |
| Configuration Time Machine | Browse prior revision snapshots and evidence validity | Persistent audit history |
| Change Ripple Preview | Explain which conclusions become stale if a field changes | Dependency graph |
| Contradiction Detector | Block proposals from experts that conflict | Graph and proposal manager |
| Read-Only Background Diary | With explicit opt-in, gather passive events outside guide UI | Session policy/storage budget |
| Offline Session Resume | View previous reports after disconnect without authority to edit | Stored snapshots |
| Small-Screen Delta Sync | Send bounded paginated summaries to 3DS without affecting USB cadence | Backpressure tests |
| Device Lease Status | Show current controller and when authority is revoked | Authenticated lease |
| Failure Reason Explainer | Explain exact stop cause rather than generic offline | Transport/auth state |
| Exportable Calibration Intelligence Report | Evidence, gaps, revision, source and unknown sections | Report generator |
| INI Coverage Without Fake Completion | 100% accounted vs 100% validated clearly distinguished | Inventory and stage states |

## Feature maturity ladder

`INVENTORIED -> EXPLAINED -> DIAGNOSABLE -> REPLAY_TESTED -> BENCH_VERIFIED -> PROFESSIONALLY_VALIDATED`. Only a separate permissions/transaction design can enable a `WRITABLE` label, and it is **not** implied by this ladder.
