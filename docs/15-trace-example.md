# End-to-end traced recommendation (illustrative)

**Scenario:** Manual editor shows a warm idle that varies in RPM. The ESP has observed ECU telemetry but has not been authorized for any write.

1. **Fingerprint**: `DefinitionSnapshot D`, `ConfigurationRevision R` verified.
2. **Active strategy**: INI inventory and current configuration reveal closed-loop idle supported/enabled; both command and feedback are represented by known valid signals.
3. **Observation**: the device reports engine-idle state, RPM target/actual, actuator command and other available diagnostic signals. Sample timestamps are recorded.
4. **Exclusions**: discard stale, sensor-invalid, startup transition or unknown-control-mode samples **for this expert**. Preserve them for other appropriate experts.
5. **Hypotheses**: actuator authority limitation; control-loop instability; fuel/combustion disturbance; changing load; sensor error.
6. **Discrimination**: analyze command/RPM temporal ordering and actuator limits; compare stable windows with correction activity. If command or status is unavailable, the hypothesis remains unresolved.
7. **Finding**: `IDLE_RESPONSE_UNRESOLVED`, attached evidence, missing checks, and `HOLD` as action if the current data is ambiguous.
8. **UI**: Guided Tuning shows "Why this matters", "Supporting data", "Other possible causes", and "Review idle controls in Manual Tuning".
9. **Manual action**: user changes a setting in the original editor. Guided Tuning cannot claim that change was suggested or verified merely because it occurred.
10. **Revision update**: R -> R+1 invalidates specific idle evidence that depends on the changed field, retains past history and waits for new eligible observations.
11. **Review**: a qualified user reviews a new comparable observation set. The advisor marks validation only if the prerequisite rules and evidence criteria are actually satisfied.
12. **Persist**: no flash write is triggered by the advisor. Saving remains separate explicit ECU/manual workflow.

### What this scenario does not establish

A stable RPM trace does not prove correct fueling, knock safety, output-driver configuration, or that every idle calibration table is optimal. It is a narrowly supported subsystem observation, not a universal pass.
