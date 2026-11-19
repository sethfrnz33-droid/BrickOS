# INI discovery, coverage and semantic registry

The INI supplies structural definitions and UI metadata; actual firmware configuration determines active strategies. A human-reviewed semantic **ruleset** adds behavior, dependencies, verification methods and write policies. Treating a name substring as a safe calibration rule is forbidden.

## Discovery pipeline

1. Obtain live ECU signature, firmware identity, capabilities and page lengths; resolve the INI by canonical signature/layout fingerprint, not arbitrary filename prefixes. Exact hash/layout must match. Fail closed if uncertain.
2. Parse INI sections into typed records: pages/commands, scalar/bit fields, arrays, curves, 2D tables, enumerations, dialogs, menus, conditionals, gauges, output channels and read-only indicators.
3. For each record retain original INI name, label, page, offset, encoding, signedness, dimensions, scale, translation, unit, declared limits, conditionals and source span. **Retain unknown directives** with diagnostics; do not silently discard them.
4. Read relevant configuration through verified transport, validating payload sizes, page boundaries and scales. The INI is not proof a feature is enabled.
5. Build immutable `DefinitionSnapshot` and `ConfigurationSnapshot`; apply a semantic rule registry matched to the fingerprint, feature guards and field identities.
6. Resolve active strategies from actual enum/flag values, including alternative load calculation, separate idle tables, injector models, closed loop, DBW, VVT and protection strategies.
7. Compute a complete inventory: accounted active, inactive, UI-hidden, not applicable, unrecognized, invalid or unknown. Each field has precisely one inventory status and zero or more semantic roles.
8. Expose `coverage`: counts and explicit unresolved identifiers. Never display "100% tuned" because all INI fields were parsed.

## Field schema (logical design)

```
FieldIdentity: definition_hash, page_id, source_name, field_index
Storage: type, signedness, endian, offset, bit_range, dimensions,
         scale, translation, unit, allowed_range
Presentation: label, help, menu_path[], display_condition
Semantics: expert_id?, functional_role[], active_condition,
           dependencies[], evidence_channels[], verification_rule_id?
Policy: READ_ONLY | REVIEW_ONLY | PROPOSAL_ELIGIBLE | WRITE_WHITELISTED
Runtime: decoded_value, raw_value, read_timestamp, freshness, verified
```

## Coverage rules

- Duplicate labels across pages are distinct fields; use stable structural identity, not a label string.
- A hidden menu item remains inventoried and is not necessarily inactive.
- A field that exists but lacks runtime backing remains **not verified**.
- A field that depends on firmware-specific interpretation is **unknown** until its ruleset is checked.
- A write must confirm matching current configuration fingerprint *and* the source snapshot before a value can be applied.
- Per-bank, per-cylinder, alternate fuel/ignition map variants are separate logical resources, not collapsed into a single "trim" or "table" concept.

## Rule coverage audit

`fields_total = recognized + unsupported + unknown` should reconcile after normalized de-duplication. Emit an audit file with every raw parameter, its semantic classification and why it is (or is not) guided. A human must be able to sort `unknown`, `no expert`, `hidden`, `inactive`, `unsafe to edit`, and `missing telemetry`.

## Non-goals

The INI is not a self-describing physical engine simulator. It cannot reveal real sensor wiring correctness, exhaust leakage, injector data quality or optimal spark timing. A dictionary of tunable variables must not be mistaken for a validated calibration method.
