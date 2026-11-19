# Proteus F7 Generated INI — Exact-Source Research Audit

**Date:** November 19, 2026. **Status: uploaded-definition research, not deployed guide firmware or a live ECU match.** This document is based directly on the user's supplied file `rusefi_proteus_f7(3).ini` (943,745 bytes, 14,487 lines, SHA-256 `24f696ac500cf48a23d3332a235654442789dac3fe08948656a36da8c320eedd`). This is the first real **full generated** target INI supplied for the BrickOS research; it resolves significant missing details, but is **not** proof that a connected ECU reports the signature or that any physical equipment is present.

The INI declares:
```
signature  = "rusEFI master.2026.11.09.proteus_f7.2735508644"
endianness = little
messageEnvelopeFormat = msEnvelope_1.0
ochGetCommand = "O%2o%2c"
ochBlockSize = 2248
blockingFactor = 1024
defaultRuntimeRecordPerSec = 100
```
The 100 value is a preferred TunerStudio runtime logging setting, **not measured sustained ECU sampling/USB throughput**; specialized tooth capture is separate.

## Complete generated-file inventory

The automated, strict-prefix research scanner found:
- **24 INI sections**; `[Constants]`, `[OutputChannels]`, `[TableEditor]`, `[CurveEditor]`, `[Menu]`, `[UserDefined]`, `[LoggerDefinition]` and more.
- **1,977 typed calibration definitions**: 766 scalar, 746 bits, 438 array and 27 string entries. These are **declarations, not verified enabled physical features**.
- **1,081 typed ordinary output-channel entries**: 788 scalar, 293 bits. An additional set of expressions, indicators and virtual channels exists in other sections; do not treat these as raw output block fields.
- **116 table editor entries**, **218 `subMenu` entries**, **124 `commandButton` entries**. The latter are operational/special-command controls, **not safe automated tutorial actions**.
- **Five anomalous I²C-related bit definitions** with an extra empty comma between type and offset, such as `i2c1_speed = bits, U08, , 3864, [0:2]` (lines 1571, 1575, 1579, 1583, 1588). A strict parser must report them and determine actual grammar compatibility; **do not silently shift offsets**.
- The source contains non-UTF8 bytes (e.g. legacy extended characters); textual decoding needs a clear policy. Byte-based field offsets remain unchanged.
- Four confirmed pairs of **display-only `noMsqSave` aliases sharing identical stored bytes**. The tuning/learning engine must canonicalize storage identity to avoid double-counting/revising the same cell.

Research inventories and example field records are in [machine-readable source audit](../design/ini/proteus-f7-2026-11-09-source-audit.v1.json). The full per-field research manifest was produced locally from the uploaded INI rather than committing the customer's raw file to the repo.

## Generated page layout (real for this INI)

| Ordinal | Identifier | Size | Contents | INI burn command |
| --- | --- | ---: | --- | --- |
| 1 | `0x0000` | 17,816 bytes | Persistent settings, primary fuel/ignition data | Present |
| 2 | `0x0100` | 256 bytes | Scattered high-speed offset list, 128 × U16 | **Empty** |
| 3 | `0x0200` | 2,048 bytes | Two 16×16 LTFT bank tables; `noMsqSave, controllerPriority` | **Empty** |
| 4 | `0x0300` | 1,268 bytes | Secondary VE, spark, torque tables | Present |
| 5 | `0x0400` | 48,000 bytes | Lua script text | Present |

Page 3 read-only LTFT ownership is especially important: do **not** assume LTFT tables may be changed by TunerStudio merely because this INI declares generic `pageValueWrite` formats for each page. The presence of Lua/GDI/secondary-table configuration is **not proof** that these features are physically equipped, selected, or safe for autonomous use.

## Real tables and canonical addresses

| Role | Page | Byte offset | Type and shape | INI line | Guide interpretation |
| --- | ---: | ---: | --- | ---: | --- |
| Primary VE | 1 | 8964 | U16 16×16, 0.1% | 1770 | Only if active fuel strategy/VE model verified |
| Primary VE load axis | 1 | 9476 | U16 ×16 | 1771 | Dynamic units via `veOverrideMode` and `fuelAlgorithm` |
| Primary VE RPM axis | 1 | 9508 | U16 ×16 | 1772 | RPM |
| Idle VE | 1 | 5884 | U16 4×4, 0.1% | 1735 | Separate map, not generic VE |
| Idle VE RPM axis | 1 | 5876 | U08 ×4, factor 10 | 1733 | Not same representation as primary bins |
| Idle VE load axis | 1 | 5880 | U08 ×4 | 1734 | Separate load convention |
| Second VE | 4 | 0 | U16 16×16, 0.1% | 142 | Present in definition; active state unverified |
| Second VE axes | 4 | 512 / 544 | U16 ×16 each | 143–144 | Conditional scaling |
| Target lambda/AFR | 1 | 9540 | U08 16×16 | 1773 | Display conditional `useLambdaOnInterface` |
| Target load/RPM axes | 1 | 9796 / 9828 | U16 ×16 each | 1774–1775 | May differ from VE axes |
| Primary ignition | 1 | 8388 | S16 16×16, 0.1 deg | 1767 | Base table, not necessarily actual spark |
| Second ignition | 4 | 604 | S16 16×16, 0.1 deg | 150 | Must verify active strategy |
| LTFT bank 1 / 2 | 3 | 0 / 1024 | F32 16×16 each | 137–138 | Read-only as defined by INI |
| Low-pressure target | 1 | 16808 | U16 4×4, 0.1 kPa | 2097 | Menu gated to `fuelPumpMode == 2` |
| Pump base-duty | 1 | 16856 | U16 4×4, 0.1% | 2100 | Pump control vs measured rail pressure remains distinct |

**Exact shared storage aliases (confirmed):**
- `wueAnalyzeTargetTable` and `lambdaTable`: page 1, offset 9540, 256 bytes.
- `wueAnalyzeCltFuelCorr` and `cltFuelCorr`: page 1, offset 5980, 64 bytes.
- `targetMixtureAdder1Display` and `targetAfrBlends1_table`: page 1, offset 15756, 128 bytes.
- `targetMixtureAdder2Display` and `targetAfrBlends2_table`: page 1, offset 15944, 128 bytes.

**No actual table contents were supplied.** These are definitions of **where/how** to decode calibration bytes, not a measured map, tune revision, guaranteed table activity, or recommended VE values.

## Actual sensor and diagnostic channels we can bind *conditionally*

These are output-block fields from this INI, **not live measurements**. Each sample still requires valid ECU signature, byte bounds, physical sensor validation, operating context and timestamp.

| Purpose | Actual INI channel / byte offset | Scale and distinction |
| --- | --- | --- |
| RPM / TPS / MAP / CLT / battery | `RPMValue` 4, `TPSValue` 22, `MAPValue` 30, `coolant` 14, `VBatt` 36 | respective native scales: 1, 0.01%, 1/30 kPa, 0.01°C, 0.001V |
| Lambda bank 1 / 2 | `lambdaValue` 34, `lambdaValue2` 218 | 0.0001 lambda; bank/sensor assignment not physically proved |
| Lambda readiness | `lambdaCurrentlyGood` bit 0 in U32 at 2088 | Must be valid before fuel inference |
| Live target mixture | `targetLambda` 942, `targetAFR` 944 | distinct scales 0.0001 lambda / 0.001 AFR |
| Fuel feedback | `Gego` 678 | U16, display factor 0.01, percent convention |
| Short-term trim bank 1 / 2 | `stftCorrection1` 1496, `stftCorrection2` 1500 | F32; INI display scale 100, translate −1; **not the same presentation semantics as Gego** |
| Long-term trim bank 1 / 2 | `ltftCorrection1` 2140, `ltftCorrection2` 2144 | F32, scale 100, translate −1; independent of LTFT page table |
| State / synchronization | `engine` 814, `totalTriggerErrorCounter` 116, `m_hasSynchronizedCrank` bit 1 / `m_hasSynchronizedPhase` bit 0 at 1804 | State bitmask vs error count vs explicit sync status are **different evidence** |
| Idle | `idleTarget` 1872, `currentIdlePosition` 1904, `isIdling` bit 14 and `isIdleClosedLoop` bit 13 at 1868 | Target/feedback/active flags |
| Ignition | `baseIgnitionAdvance` 984, `correctedIgnitionAdvance` 986 | S16, 0.02°; cannot prove physical ignition timing |
| Knock | `m_knockRetard` 1036, `m_knockCount` 1044 | One is a retard measure, the other is a **count**; neither establishes MBT |
| Low-pressure fuel | `lowFuelPressure` 106, `fuelPressureTarget` 1342, `fuelPumpDuty` 1340 | Measured channel as reported vs configured target vs pump *command* |
| Pump feedback active | `isFpPidActive` bit 5 at 1336 | Only meaningful if relevant strategy enabled |
| Physical safety diagnostics | `hasCriticalError` bit 14 at 0, `isMapValid` bit 24 at 0 | Source-reported statuses, not full vehicle safety certification |

The output block declares `ochBlockSize=2248`, so a complete ordinary output read spans multiple 1024-byte protocol chunks under the configured maximum. This **does not establish** actual 100 Hz end-to-end performance. The INI separately defines **Composite Logger**, **Primary Tooth Time Logger** and **Trigger Oscilloscope** in `[LoggerDefinition]`; those streams need their own scheduling, record-length parsing and firmware support check.

## Verified semantic mismatches against current browser Guide

BrickOS `firmware/esp32/web/guided.mjs` currently uses a **first-matching alias** policy. Cross-checking its 25 role aliases against all 1,081 typed output fields in this generated INI shows:

1. **Spark mismatch:** no match for `actualLastInjectionIgnitionAngle`, `ignitionAdvance`, `timingAdvance` or `actualIgnitionAdvance`; actual channels are `baseIgnitionAdvance` and `correctedIgnitionAdvance`. Browser spark evidence would be **missing**, not 0.
2. **Fuel pressure mismatch:** `fuelPressure` channel is absent; correct candidate is `lowFuelPressure` for low-pressure supply (not `highFuelPressure`, which is a separate GDI-related source).
3. **Sync meaning mismatch:** browser `sync` resolves to `totalTriggerErrorCounter`, which measures error count, **not currently synchronized**. Separate sync flags exist in the generated INI.
4. **Knock meaning mismatch:** browser's first matching alias is the `m_knockCount` counter, not degrees of retard; `m_knockRetard` is a distinct channel. Do not interpret the count as a numerical retard amount.
5. **Idle target:** browser's first two aliases don't exist, but its fallback `idleTarget` does. `currentIdlePosition` also exists. Resolve using canonical identifiers, not loose naming.
6. **Fuel-load model:** current browser's generic 8×8 MAP/RPM evidence bins are **not** the true primary/idle/secondary lookup shapes/active axes in this INI. No successful VE application can be established until the relevant strategy is confirmed.

This audit **does not change** the existing browser, ESP runtime or DS connectivity; it supplies exact, source-verified research targets for the next implementation phase.

## HPA-style guide chapter linkage

| Stage | Actual INI family | What remains unresolved |
| --- | --- | --- |
| 1: Setup | `engineType`, `cylindersCount`, `displacement`, pin selectors, sensor calibrations | Physical wiring, actual connected ECU and engine archive |
| 2: Trigger | `trigger_type`, `globalTriggerAngleOffset`, sync bits/errors, optional logger | Physical trigger reference and wiring |
| 3: Base tables | `fuelAlgorithm`, primary/idle/secondary VE, ignition, injector characterization | Active selection, hardware matching and valid starting calibration |
| 4: References | `fuelPumpMode`, `fuelPressureSensorMode`, pressure-target and reference fields, timing offset | Regulator topology and independently measured physical results |
| 5: Starting | `engine` state bitmask, cranking/after-start corrections, sensor readiness | A recorded event and independently verified hardware |
| 6: Idle | `idleTarget`, `currentIdlePosition`, idle-active bits, idle VE and warmup | Actual idle strategy, operating mode and valid observations |
| 7: Steady fuel | measured/target lambda, Gego, STFT/LTFT, active VE interpolation | Qualified samples, bank mapping, pressure/injector model, sensor delay |
| 8: Steady ignition | base vs corrected spark, knock counts/retard, corrections | Independent torque and physical timing verification |
| 9: External validation | source-specific logger, fault/protection channels | Qualified independently recorded evidence; no automated power testing |
| 10: Confirm | revision/profile/INI state and evidence history | What remains incomplete, not `vehicle safe` or `optimal` verdict |

The generated INI references certain GDI/high-pressure/fuel features, but **BrickOS Guided Fuel Tuning remains PFI-only**. A menu name or page definition is not a product capability override.

## Implementation gates established by this exact INI

1. Capture actual ECU query signature/firmware response and verify equals this file's signature/hash-compatible definition before any source-based diagnosis.
2. Build parser fixtures for **all five page records**, including literal endianness and command formatting; reject invalid bounds or unknown conditional expressions.
3. Resolve canonical fields with semantic roles, units, transformations, active conditions and `noMsqSave` alias grouping.
4. Verify all source roles against real decoder output; do **not** treat `rpm`/sync/knock/fuel-pressure loose aliases as interchangeable.
5. Parse `[LoggerDefinition]` separately from ordinary output channels; qualify actual delivered rate and timestamps on target hardware before calling it high-rate diagnostics.
6. Distinguish **read-only** tables/indicators, command-button actions, physical-hardware evidence, and config elements requiring ECU state/power cycle; no automation through `ControllerCommands` without a separate reviewed design.
7. Use actual 16×16 and 4×4 axes for any review-only VE coverage; isolate actual bank/strategy and measured/target lambda validity.
8. Keep full guide teaching content on the ESP and portable across phone/DS; record missing input as `unknown`.
9. Preserve open questions about exact physical engine hardware (injection topology, pressure sensing, pump regulator, wideband installation, trigger), and keep **GDI/mixed unsupported**.
10. No ECU RAM/flash command changes or engine operations are authorized by this research.

**Next prerequisite:** a read-only ECU handshake/tune snapshot **from a verified matching connected ECU** plus measured ESP memory/transport conditions. Without these, this source confirms representation, not active calibration values or hardware readiness.

## Supporting repository artifacts

- [Structured source audit and selected exact role mappings](../design/ini/proteus-f7-2026-11-09-source-audit.v1.json)
- [General INI provenance and source-quality policy](40-verified-external-source-ledger-and-rusefi-constraints.md)
- [Implementation prerequisites and policy](41-implementation-readiness-decisions-and-traceability.md)
- [Seth's education spec](36-seth-complete-teaching-and-tuning-master-spec.md)

*This document contains original analysis and summary, not copied TunerStudio lessons. It references only user-supplied INI data and the existing BrickOS source.*
