# Guided Tuning shared state, message contract and acceptance tests

**Planning only.** All routes below are proposed interfaces; they do not describe existing ESP firmware. Neither the currently imported ESP v4.8 firmware nor the standalone Python prototype can perform the complete guided workflow.

## Shared, revisioned data objects
A client renders only ESP-owned objects and does not infer ECU settings from untyped values:
- `DefinitionIdentity`: ECU signature, INI hash, firmware/build identity, schema/capability version, calibration revision.
- `ChannelSample`: channel ID, native units and conversion rule, source clock/ESP receive timestamp, validity/freshness, bank, applicable ECU mode, data revision.
- `EvidenceWindow`: immutable sample references, acceptance/rejection reasons, time alignment, operating-state classification and source snapshot.
- `Finding`: stable ID, expert ID, observed facts, competing hypotheses, missing evidence, severity, explainability trail, user-facing summary, related INI field IDs.
- `Stage`: named engine-lifecycle stage, applicability, dependency IDs, evidence status and measured coverage.
- `Proposal`: *never* an authorized edit; includes source revision, original value and suggested candidate with uncertainty, cell scope, locks and counterevidence.
- `ControllerLease`: authenticated owner, short lifetime, heartbeat expiry, scope, session nonce, config revision and revocation reason.
- `AuditEvent`: server sequence, action, actor, exact ECU response where applicable, timestamp, config revision, abort reason, hash linkage.

Clients must use stable IDs and incremental cursors; asynchronous responses are ignored if their source revision is older than what the client already displayed. Disconnection marks existing displayed measurements stale but does not erase histories.

## Display/control message flow
```
       ESP                                                3DS / PHONE
       |--- GET capabilities / version --------------------->|
       |<-- matched identity and read-only capabilities ------|
       |--- GET overview cursor ----------------------------->|
       |<-- current stage, top task, status, cursor ----------|
       |--- GET paged evidence (on demand) ------------------>|
       |<-- bounded samples, reasons, provenance -------------|
       |--- authenticated request: learning session ---------->|
       |<-- lease token + expiry + role (if eligible) --------|
       |--- periodic heartbeat / session renewal ------------>|
       |<-- only explicit lease/state acknowledgement --------|
       |--- read-only proposal query ------------------------>|
       |<-- candidates, never implicit ECU writes ------------|
       |--- disconnect or expiry ---------------------------->|
       |<-- immediately suspend new learn activity -----------|
```
The arrows describe request/response categories, not a finalized wire encoding. ECU acquisition does not synchronously wait for the device frame rate.

## Failure-class behavior
| Failure or conflict | Required ESP action | Display |
| --- | --- | --- |
| Controller heartbeat expires | suspend new guided learning/operations; revoke lease | `SESSION ENDED — connection lost` |
| Wi-Fi drops during passive browsing | continue only explicitly enabled passive read-only observation | last data tagged stale |
| ECU USB transport fails | reject new samples/actions, record partial transactions | `ECU OFFLINE` and last-confirmed revision |
| Matching INI changes | stop affected experts, invalidate dependent proposals | `Definition changed — recheck` |
| Manual tune changes same field | invalidate conflicting proposal; preserve manual edit | `Manual change detected` |
| Fuel sensor invalid / lambda stale | exclude sample and suspend VE inference | exact reject reason |
| STFT/LTFT native scaling unresolved | preserve raw readings but block VE conversion | `Trim interpretation unavailable` |
| External writer modifies calibration | suspend any pending operation and request reconciliation | `Calibration revision conflict` |
| Phone opens while 3DS owns lease | phone is read-only spectator | `Controlled by another device` |
| Engine reports critical fault | stop relevant learning; surface ECU's fault | `Learning paused by ECU fault` |
| Proposed adjustment exceeds verified scope | refuse proposal/action | which constraint failed |
| App restarted | enter read-only mode until explicitly authenticated | `Learning not active` |
| ESP rebooted | all volatile leases invalid, recovery audit available | `Session interrupted` |
| Save/burn requested from passive guide | reject; no implicit persistent operation | `Use explicit save workflow` |

**A heartbeat is not permission.** Real authorization requires a verified pairing/identity scheme, replay protection and operation-scoped permissions; the existing open Wi-Fi AP must not be allowed to authorize writes just because a device can reach it.

## Required automated test matrix before real firmware integration
### Definition and eligibility
- [ ] Test multiple generated rusEFI INIs, signatures and page layouts without guessing unknown field types.
- [ ] Verify every parsed field is accounted for even when hidden by conditional menu logic.
- [ ] Reject mismatched signatures, stale layouts, out-of-range pages, wrong units and unsupported secondary tables.
- [ ] Test separate idle VE, Alpha-N versus Speed Density and dual-bank trim availability as independent strategy variants.

### Evidence / engine states
- [ ] Replay startup events with genuine sensor-ready and sensor-not-ready periods.
- [ ] Verify ASE/WUE evidence cannot be attributed to steady VE without active-state validation.
- [ ] Replay stable, transient, DFCO and recovery periods and count accepted/rejected observations.
- [ ] Verify measured versus target lambda is unit-safe and bank-aware.
- [ ] Verify STFT/LTFT/Gego are never silently treated as interchangeable scales.
- [ ] Verify interpolation distributes observations to the actual table influence region.
- [ ] Verify duplicate samples do not masquerade as independent visits.
- [ ] Verify temperature, timing, electrical and missing-channel uncertainty demotes confidence.
- [ ] Verify a changed axis, sensor calibration or manual edit invalidates affected historical conclusions.

### Session / concurrency
- [ ] Lost heartbeat and explicit stop prevent any further guided operation.
- [ ] A stale response/replayed token or unpaired client cannot resume a session.
- [ ] Two clients observing is allowed; two simultaneous controllers are denied.
- [ ] ESP reboot never resurrects a previous controller lease.
- [ ] Transport timeout during any future write produces an exact `unknown outcome` status; never claim success.
- [ ] No edit path exists in the initial read-only guided release.
- [ ] A future RAM write transaction requires separate specialist-reviewed safeguards, exclusive writer, verified snapshot and readback.
- [ ] Burn/save is never invoked by accepting a learning proposal, stopping or leaving a screen.

### Interface
- [ ] 3DS two-screen workflow maps one shared finding to top evidence and bottom navigation.
- [ ] Phone portrait/landscape render same task ID and explanation as 3DS.
- [ ] Every task links to original INI manual editor without altering it.
- [ ] Disabled controls show a reason; missing input never appears as zero.
- [ ] Stale/offline labels show on both clients.
- [ ] Measured, calculated, estimated and unknown are distinct in API and UI.
- [ ] All advanced filters have descriptive units, validation, reset and profile-version semantics.
- [ ] Leaving Guided Tuning keeps ordinary tuning and View-tab workflows unchanged.

## Shipping gates
**Gate A — read-only engine lab:** ECU identity + complete INI accounting, deterministically replayed evidence and a stable API across both clients. Hardware changes are impossible.

**Gate B — guided advisory:** first-start, idle, warmup, trim, VE and transient experts explain findings and produce traceable *non-executable* proposals; demonstrated on recorded/simulated datasets.

**Gate C — approved RAM workflow (future):** separate threat model, ECU-version-specific transaction proof, security, concurrency/rollback handling, readback and professionally supervised hardware validation.

**Gate D — persistent save and instrumented calibration (future):** distinct approval and audit of the ECU's exact flash-save semantics. No unqualified automated ignition-advance capability.

This acceptance matrix is a specification checklist, not evidence that any of its items have passed.
