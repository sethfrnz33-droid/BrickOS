# Expert-module registry and extensibility

## Expert declaration

Each rule-bearing calibration expert registers:

- Semantic IDs and firmware compatibility fingerprints;
- Required and optional INI fields, output channels and feature flags;
- Active-condition expression and graph edges;
- Valid operating states and exclusion/rejection predicates;
- Measurements, evidence collection window and confidence components;
- Candidate explanations and tests to distinguish them;
- Read-only findings and UI links to original manual editor;
- A separately reviewed proposal policy, if any, and independent validation requirements.

The expert does **not** own the bus, session authorization or flash save command. Shared infrastructure owns those responsibilities.

## Core experts (first release)

1. ECU identity / compatibility.
2. Basic electrical and sensor plausibility.
3. Trigger/sync diagnostics.
4. Cranking, first-start and catch events.
5. After-start / warmup.
6. Idle actuator strategy / authority.
7. Idle fuel and correction masking.
8. Closed-loop idle control diagnosis.
9. Primary fuel strategy / table mapping.
10. Bank-aware trim accounting.
11. Transient and decel event classifier.
12. Spark correction and knock diagnostics.
13. Motion source quality.
14. Professional dyno log ingestion / repeatability.
15. Protection and warning coverage.
16. Whole-config audit / unresolved INI fields.

## Extension/compatibility rule

An unknown ECU field enters the inventory as `NO_VERIFIED_SEMANTICS`, even if its name resembles a supported field. Additional experts can be introduced by semantic manifest after review and testing. Version-specific exceptions remain versioned, testable records rather than loose substring matching.

## Cross-expert conflict resolution

The coordinator deduplicates findings by underlying cause, applies the dependency graph and refuses simultaneous contradictory proposals. For example, steady-state fueling and transient enrichment experts should not "correct" the same short event. Sensor plausibility conflicts suspend both until resolved. A single measurement can support multiple hypotheses but cannot be counted as multiple independent pieces of evidence.

## Diagnostics coverage

Unknown parameters never disappear: a dedicated `Configuration Atlas` expert makes them searchable and provides links to the original settings, source help text and reason automatic guidance is unavailable.
