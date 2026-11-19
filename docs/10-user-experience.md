# Guided Tuning UX — separate from Manual Tuning

## Top-level entry points

```
BRICK TUNER SUITE
  [Manual Tuning]  -> unchanged tree, tables, gauges, write controls
  [Guided Tuning]  -> independent navigation, evidence and recommendations
  [Logging]        -> logs and history
```

Guided Tuning has five permanent tabs: **Next**, **Systems**, **Evidence**, **Atlas**, **History**. The existing manual UI is never hidden, disabled or reordered because Guided Tuning exists. A task may link into the exact original dialog/table and return to its originating finding afterward.

## Next

Show one main recommendation and at most a few clear competing issues. Include `what I saw`, `why it matters`, `other causes`, `what is missing`, and an actionable next step. Explain blockers instead of simply greying things out. Keep task identity stable to prevent rapidly changing prompts.

## Systems

Display engine lifecycle stages and major system experts with evidence status (not percentage tuned). Show optional strategies only if enabled and supported. A `blocked` stage displays the actual dependency. Allow experts to navigate out of order in read-only mode.

## Evidence

Channel badges for present/fresh/verified/unavailable and graphs of relevant time windows. Separately expose source timestamps, corrections, excluded samples and confidence components. A region heatmap uses descriptive coverage labels, not invented safety grades.

## Atlas

Reconstruct the INI tree including hidden or unsupported fields. Filter unknown, inactive, critical, recently changed and missing evidence. Each field has a raw definition identity, current value, applicability, owning expert, dependencies, explanation and a **Go to Manual Setting** action. Unresolved semantics are explicitly labelled.

## History

Persistent start events, findings, proposal previews, configuration revisions, user acknowledgements, validation outcomes and session disconnect reasons. Allow snapshots and comparisons without altering active ECU settings.

## Device constraints (3DS)

A 3DS is a limited presentation device. Keep rendering off the acquisition critical path, send compact status deltas at a rate the client can sustain, support pagination for the Atlas and supply on-demand windows for charts. Let a disconnected device reconnect for *read-only browsing*, but never treat reconnection as restoring an authorization to perform edits.

## UX microcopy standard

Say: `Measured`, `Reported by ECU`, `Calculated`, `Estimated`, `Needs external verification` or `Unknown`. Do not say `optimized`, `safe`, `MBT found` or `perfectly tuned` when the supporting evidence is absent.
