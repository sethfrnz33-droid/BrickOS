# Seth Full Tour — Ten-Stage Curriculum-Aligned Guide

**Status:** The phone browser catalogue contains the revised read-only dialogue sequence. The Python prototype, ESP firmware-side learning controller, and DS dialogue client are separate future work. No engine operation or calibration writes result from this guide.

## Origin

- Reference: [High Performance Academy Practical Standalone Tuning — public curriculum](https://www.hpacademy.com/courses/practical-standalone-tuning/) (reviewed November 19, 2026).
- BrickOS follows the public ten-stage ordering, not proprietary course lessons or any specific fuel/ignition settings.
- Seth delivers original plain-language explanations sourced from the matched ECU/INI, with unknown physical facts explicitly requiring independent verification.

## Chapter navigation

| # | Chapter | Catalogue node | Expert binding |
|---|---|---|---|
| 1 | ECU configuration and testing | `tour.configuration` | `identity` |
| 2 | Trigger setup | `tour.trigger` | `trigger` |
| 3 | Base table configuration | `tour.base_tables` | `fuel` |
| 4 | Base ignition / fuel pressure verification | `tour.reference` | `spark` |
| 5 | Initial startup | `tour.starting` | `starting` |
| 6 | Idle tuning | `tour.idle` | `idle` |
| 7 | Steady-state fuel | `tour.steady_fuel` | `fuel` |
| 8 | Steady-state ignition | `tour.steady_spark` | `spark` |
| 9 | Full-power instrumented review | `tour.full_power` | `validation` |
| 10 | Final confirmation | `tour.confirm` | `validation` |

## What stays flexible

- The ten chapters are a **navigation structure**, not ten compulsory operations or evidence of a verified tune.
- The ECU-matched INI determines actual settings, output-channel values, menu deep links and capability availability. The dialogue contains **no fixed page offsets**.
- When a required channel is unavailable, Seth says so rather than treating zero or an assumed default as a reading.
- Warmup, fuel trims, transients, optional subsystems and the complete field-by-field INI Atlas remain accessible as dedicated subtopics.
- Targeted Help goes to existing topic-specific experts; it does not duplicate the Full Tour's calibration logic.
- The phone browser remains the initial renderer; the DS/DSi networking and manual editors remain unchanged.
- Physical reference verification and high-load assessment are **professional instrumented review topics only**. No automated actuator test, ignition advance, dyno run or saving operation is initiated by the guide.

## Portable dialogue fields

- Every fixed node has a stable string `id`, text, UI choices, owning expert and optional `roles` resolved via INI channel metadata.
- Catalogue v1.1.0 adds ordered `tour_steps` with `step`, `id`, `title`, `node`, `expert` and related roles.
- The shared interpreter adds optional packet fields `tour_step`, `tour_total` and `tour_title`; a DS reader can ignore these until it supports the tour badge.
- Catalogue updates are fetched from the ESP. A definition or catalogue-version mismatch invalidates stale resume state; it never authorizes a calibration change.
- Unknown INI fields get conservative field-information pages and can deep-link to the pre-existing editor only when an actual menu target is resolved.

## Verification criteria

1. All ten chapter nodes exist, form a connected progression, and retain an independent Quick Help path.
2. Every displayed chapter number agrees with the ESP-served tour metadata.
3. Unmatched or stale INIs cannot supply presumed values or routes.
4. All existing manual editor and DS connection behavior stays unchanged.
5. Host tests exercise the ten-chapter sequence; hardware operation and ESP-IDF compilation need separate validation.

**Explicit limitation:** Educational navigation and evidence reviews do not prove that an ECU calibration is safe or complete.
