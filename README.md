# BrickOS — Guided Calibration Architecture

**Status:** Design specification / research / unimplemented roadmap.  
**Baseline inspected:** BrickOS ESP firmware v4.12 (read-only VE learner, fuel-trim reader, seven-stage guide, optional motion receiver).  
**Audience:** Brick ECU developers and experienced calibration engineers.  
**Product rule:** Manual Tuning is the default experience; Guided Tuning is a separate, opt-in mode.

## Product definition

BrickOS is a configuration-aware ECU tuning platform built around rusEFI. The proposed **Guided Calibration** subsystem interprets an ECU-matched INI and the *actual active configuration*, converts its configuration into a dependency graph, assesses quality of measurements, produces transparent explanations and an ordered list of outstanding calibration tasks, and preserves an auditable session history. It does not promise an automatically perfect tune or automatically infer ignition MBT from indirect signals.

A connected 3DS can be the interface, but the ESP remains the authority for state, evidence, authorization and ECU communication. Wi-Fi presentation may be slow; ECU data acquisition should be as fast as the transport can reliably support without compromising ECU responsiveness.

## Essential invariants

1. **Never interfere with Manual Tuning.** The advisor never silently rewrites manual edits, reorders the existing manual UI or hijacks ECU control.
2. **No unapproved changes.** The ESP is read-only unless a narrowly scoped, verified write capability is explicitly enabled and authorized. No autonomous spark optimization or first-start actuation is assumed.
3. **Exact ECU definition.** Verify the live ECU signature/layout and actual pages before decoding or writing, independent of arbitrary INI filenames.
4. **Explain every conclusion.** Every finding has sources, supporting samples, exclusions, competing explanations, uncertainties and a next action.
5. **Account for the complete INI.** Every discovered item is inventoried, including irrelevant, hidden, inactive and unsupported entries; not every item is automatically optimizable.
6. **Protect data integrity.** Calibration revision changes invalidate dependent evidence. Unknown concurrent changes block writes, not observation.
7. **No false safety confidence.** Sensor/data quality and coverage never equal engine safety or MBT proof.
8. **Professional validation.** First-start, higher-load, knock, ignition and dyno work require appropriate instruments and qualified supervision. No unattended on-road calibration experiments.

## Documentation index

- [Current status and boundaries](docs/00-current-status.md)
- [INI discovery and semantic coverage](docs/01-ini-discovery.md)
- [Graph and dependency invalidation](docs/02-dependency-graph.md)
- [Telemetry and evidence quality](docs/03-observation-engine.md)
- [Decision engine, recommendations and states](docs/04-reasoning-engine.md)
- [Full engine-lifecycle workflow](docs/05-engine-lifecycle.md)
- [Calibration expert registry](docs/06-expert-modules.md)
- [Fuel, VE, trims and transient intelligence](docs/07-fueling.md)
- [Starting, warmup and idle intelligence](docs/08-start-idle.md)
- [Ignition, IMU and dyno evidence](docs/09-smart-spark.md)
- [3DS/web guided user experience](docs/10-user-experience.md)
- [ESP / clients protocol contract](docs/11-api-and-state.md)
- [Authorization, safety and persistence](docs/12-safety-and-security.md)
- [Testing and acceptance criteria](docs/13-verification.md)
- [Source verification and compatibility](docs/14-references.md)
- [Example fully traced recommendation](docs/15-trace-example.md)
- [Detailed feature catalog](docs/16-feature-catalog.md)
- [Worked decision rules](docs/17-decision-rules.md)
- [Proposal and review transaction model](docs/18-transaction-and-review.md)
- [Backlog and milestone order](ROADMAP.md)
- [Decision log](DECISIONS.md)

**Scope of this repository:** design materials only. The v4.12 firmware was inspected as baseline but is not included or modified here. Any future implementation must be validated against a real ECU/firmware INI and an independent test setup.
