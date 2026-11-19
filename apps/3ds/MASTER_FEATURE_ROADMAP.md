# Brick Tuner Suite — Master Feature Roadmap

> Source of truth for all future releases. This is a **wishlist**, not a feature-completeness claim.
> Checked = implemented in source and tested in simulation; hardware validation is independent.
> Partial = some infrastructure exists, but expected end-to-end behavior is incomplete.

## Tracking rules
- Every new release must update this file, `FEATURE_TRACKER.csv`, and `FEATURE_STATUS.md`.
- Never check an item merely because its button is drawn. Require a meaningful code path and tests.
- Distinguish host/source tests from real Nintendo 3DS / actual ECU hardware verification.
- Confirm missing INI channels instead of fabricating values; never silently overwrite calibration.
- P0 core reliability; P1 normal tuning; P2 richer workflows; P3 bench; P4 advanced.

## 01. Platform and Main Modes — P0

- [x] **BTS-001** Dashboard mode
- [x] **BTS-002** Tune mode
- [x] **BTS-003** Logging mode
- [x] **BTS-004** Analysis mode
- [x] **BTS-005** Diagnostics mode
- [x] **BTS-006** Configuration mode
- [ ] **BTS-007** Bench tools mode
- [ ] **BTS-008** Files and tune management mode
- [x] **BTS-009** Connection and performance mode
- [x] **BTS-010** Persistent global ECU status strip
- [ ] **BTS-011** Consistent back navigation and selected-row restoration *(partial)*
- [x] **BTS-012** Screen switching without blocking ECU polling
- [x] **BTS-013** Separate high-rate acquisition from rendering
- [ ] **BTS-014** Safe offline browsing with explicit offline labels *(partial)*

## 02. Dashboard and Gauges — P0

- [x] **BTS-015** Digital-only instrument layouts
- [x] **BTS-016** 2-gauge large display
- [x] **BTS-017** 4-gauge default display
- [x] **BTS-018** 6-gauge dashboard
- [x] **BTS-019** 9-channel overview
- [x] **BTS-020** 1 large primary channel
- [ ] **BTS-021** User-built gauge layouts *(partial)*
- [ ] **BTS-022** Save multiple dashboard presets *(partial)*
- [x] **BTS-023** Fast L/R gauge cycling
- [ ] **BTS-024** Stylus layout editing
- [ ] **BTS-025** Choose channel by tapping gauge
- [ ] **BTS-026** Configurable units
- [x] **BTS-027** Number precision per gauge
- [ ] **BTS-028** User-set min and max limits
- [ ] **BTS-029** Color-coded warning thresholds
- [ ] **BTS-030** Warning hysteresis
- [x] **BTS-031** Peak hold and reset
- [x] **BTS-032** Minimum/maximum capture
- [x] **BTS-033** Average gauge values
- [ ] **BTS-034** Small trend sparkline
- [ ] **BTS-035** Full-screen trend by gauge
- [ ] **BTS-036** Live RPM and load labels *(partial)*
- [ ] **BTS-037** Coolant temperature warning
- [ ] **BTS-038** Oil pressure warning
- [ ] **BTS-039** Battery voltage warning
- [ ] **BTS-040** Adjustable audible alarms
- [ ] **BTS-041** Dedicated shift indication
- [ ] **BTS-042** Display actual packet age *(partial)*
- [x] **BTS-043** Hold last valid sample on link loss
- [x] **BTS-044** Clearly mark stale gauges
- [x] **BTS-045** No fabricated values on missing samples
- [x] **BTS-046** Restore last dashboard after reboot
- [ ] **BTS-047** Readable font-size selection
- [ ] **BTS-048** Instantly inspect 30-second history
- [ ] **BTS-049** Preset fuel/ignition/boost/thermal dashboards

## 03. Tuning Tables and Map Editing — P1

- [ ] **BTS-050** [PARTIAL] Full real INI-defined table browsing *(partial)*
- [x] **BTS-051** Two-dimensional value heatmap
- [ ] **BTS-052** Optional three-dimensional value surface
- [ ] **BTS-053** Correct X/Y units and scaling *(partial)*
- [ ] **BTS-054** Reverse displayed axes without changing storage *(partial)*
- [x] **BTS-055** D-pad cell selection
- [ ] **BTS-056** Touchscreen cell selection
- [x] **BTS-057** Rectangular multiselect
- [x] **BTS-058** [PARTIAL] Multi-cell increment/decrement
- [x] **BTS-059** Percent adjustment of selection
- [ ] **BTS-060** Multiply selection by factor *(partial)*
- [x] **BTS-061** Horizontal interpolation
- [x] **BTS-062** Vertical interpolation
- [x] **BTS-063** Bilinear interpolation
- [x] **BTS-064** Smooth neighboring cells
- [x] **BTS-065** Undo edits
- [x] **BTS-066** Redo edits
- [x] **BTS-067** Copy selected cells
- [x] **BTS-068** Paste cells and ranges
- [x] **BTS-069** Compare each cell to baseline
- [x] **BTS-070** Modified-cell outline
- [x] **BTS-071** Consistent map text color
- [x] **BTS-072** Heatmap only changes cell fill
- [ ] **BTS-073** Zoom and pan map *(partial)*
- [x] **BTS-074** Keep cursor in comfortable viewport
- [x] **BTS-075** Edit X-axis bins
- [x] **BTS-076** Edit Y-axis bins
- [x] **BTS-077** Respect read-only axes
- [x] **BTS-078** Verify allowed numeric ranges
- [x] **BTS-079** Choose adjustment step with L/R
- [x] **BTS-080** Show live operating-point cursor
- [ ] **BTS-081** Interpolate the active four cells
- [ ] **BTS-082** Display current RPM and load with table *(partial)*
- [ ] **BTS-083** Display measured vs target lambda where supported
- [ ] **BTS-084** Display ignition correction breakdown where supported
- [ ] **BTS-085** Pin comparison map
- [ ] **BTS-086** Jump to particular RPM/load location *(partial)*
- [ ] **BTS-087** Quick list of favorite maps *(partial)*
- [ ] **BTS-088** Search tables from INI
- [x] **BTS-089** Show pending unburned changes
- [ ] **BTS-090** [PARTIAL] One deliberate burn-and-verify action *(partial)*
- [ ] **BTS-091** [PARTIAL] Show real burn progress and result *(partial)*
- [x] **BTS-092** Verify ECU readback after writing
- [x] **BTS-093** Backup full page before writing
- [ ] **BTS-094** Preserve edits through app interruptions
- [x] **BTS-095** Warn before leaving with unsaved edits
- [x] **BTS-096** Highlight power-cycle-required changes
- [x] **BTS-097** Prevent writes from old ECU signature
- [x] **BTS-098** No automatic blind write retries
- [x] **BTS-099** Revert individual cells
- [ ] **BTS-100** Restore saved table region *(partial)*
- [ ] **BTS-101** [PARTIAL] Offline edits to stored tune file *(partial)*
- [ ] **BTS-102** Calibration compare and merge
- [ ] **BTS-103** Table generator and interpolation preview

## 04. Fuel and Ignition Workspaces — P2

- [ ] **BTS-104** VE table direct shortcut
- [ ] **BTS-105** Target lambda direct shortcut
- [ ] **BTS-106** Injector characterization viewer
- [ ] **BTS-107** Fuel pressure monitor
- [ ] **BTS-108** Differential fuel pressure calculation with valid sensors
- [ ] **BTS-109** Fuel trim monitor
- [ ] **BTS-110** Long-term and short-term trims where exposed
- [ ] **BTS-111** Fuel correction sources breakdown
- [ ] **BTS-112** Warm-up enrichment workspace
- [ ] **BTS-113** Cranking enrichment view
- [ ] **BTS-114** Afterstart enrichment view
- [ ] **BTS-115** Acceleration enrichment view
- [ ] **BTS-116** Deceleration and fuel-cut analysis
- [ ] **BTS-117** Ignition advance main table shortcut
- [ ] **BTS-118** Live actual ignition advance
- [ ] **BTS-119** Ignition correction sources breakdown
- [ ] **BTS-120** Knock-level viewer
- [ ] **BTS-121** Knock retard and count history
- [ ] **BTS-122** Per-cylinder trim view where ECU supports
- [ ] **BTS-123** Idle ignition workspace
- [ ] **BTS-124** Ignition temperature correction viewer
- [ ] **BTS-125** Boost-related timing correction viewer
- [ ] **BTS-126** Target/actual lambda synchronized chart
- [ ] **BTS-127** Correction activation indicators
- [ ] **BTS-128** Fuel and spark session workspace presets

## 05. Logging and Replay — P1

- [x] **BTS-129** One-button start/stop logging
- [x] **BTS-130** Record status on both screens
- [x] **BTS-131** Recording elapsed timer
- [x] **BTS-132** Actual sample-rate indicator
- [x] **BTS-133** RAM recording with bounded memory
- [ ] **BTS-134** [PARTIAL] SD export of full recorded samples *(partial)*
- [x] **BTS-135** CSV export and import
- [ ] **BTS-136** Log name and session metadata *(partial)*
- [ ] **BTS-137** SD free space display
- [ ] **BTS-138** [PARTIAL] Log channel selection *(partial)*
- [ ] **BTS-139** Log channel preset groups
- [ ] **BTS-140** [PARTIAL] Event-triggered start *(partial)*
- [ ] **BTS-141** Pretrigger recording buffer
- [x] **BTS-142** Logging duration limit
- [x] **BTS-143** Live scrolling graph
- [ ] **BTS-144** Up to four traces per graph page *(partial)*
- [ ] **BTS-145** More channel pages *(partial)*
- [ ] **BTS-146** Per-channel trace colors
- [ ] **BTS-147** Independent Y scales *(partial)*
- [ ] **BTS-148** Shared Y-scale option
- [ ] **BTS-149** Zoom time range *(partial)*
- [ ] **BTS-150** Vertical zoom *(partial)*
- [x] **BTS-151** Touchscreen timeline scrubbing
- [ ] **BTS-152** [PARTIAL] D-pad sample-by-sample scrub *(partial)*
- [x] **BTS-153** Playback speed selection
- [x] **BTS-154** Pause/resume/replay
- [x] **BTS-155** Exact cursor sample values
- [x] **BTS-156** Minimum/maximum markers
- [x] **BTS-157** Persistent log bookmarks
- [x] **BTS-158** Jump to next/previous bookmark
- [ ] **BTS-159** Automatic ECU-fault bookmarks *(partial)*
- [ ] **BTS-160** Threshold-crossing search *(partial)*
- [ ] **BTS-161** Zoom to selected event *(partial)*
- [x] **BTS-162** Compare two logs
- [ ] **BTS-163** Overlay multiple saved traces *(partial)*
- [ ] **BTS-164** Log channel view presets
- [ ] **BTS-165** ECU-resident SD log list and download
- [x] **BTS-166** Log quality report
- [ ] **BTS-167** Dropped-sample estimate *(partial)*
- [ ] **BTS-168** Exports including firmware signature *(partial)*
- [x] **BTS-169** No invented sensor samples in replay
- [x] **BTS-170** Replay after ECU disconnect
- [ ] **BTS-171** Associated tune revision reference

## 06. Trace and Analysis — P1

- [x] **BTS-172** Log-to-map context linking
- [x] **BTS-173** Only use explicitly mapped INI axis channels
- [x] **BTS-174** Recorded RPM/load point on actual table
- [x] **BTS-175** Interactive log-cursor tracking
- [x] **BTS-176** Jump to map from selected log moment
- [x] **BTS-177** Map coverage counts by cell
- [x] **BTS-178** Heatmap of sampled operating regions
- [ ] **BTS-179** RPM/load scatter plot
- [ ] **BTS-180** XY plot mode
- [x] **BTS-181** Histogram of selected channel
- [ ] **BTS-182** Lambda error heatmap
- [ ] **BTS-183** Mean lambda error by table cell
- [ ] **BTS-184** Median lambda error by table cell
- [ ] **BTS-185** Minimum/maximum error by cell
- [ ] **BTS-186** Error standard deviation
- [ ] **BTS-187** Sample count and confidence visibility *(partial)*
- [ ] **BTS-188** Separate transient and steady-state samples
- [ ] **BTS-189** Exclude fuel-cut samples
- [ ] **BTS-190** Exclude invalid wideband readings
- [ ] **BTS-191** Exclude warmup samples by coolant
- [ ] **BTS-192** Filter sample range by TPS
- [ ] **BTS-193** Filter by RPM range
- [ ] **BTS-194** Filter by load range
- [x] **BTS-195** Time window selection
- [ ] **BTS-196** Summary of highly sampled regions *(partial)*
- [ ] **BTS-197** Summary of unsampled regions *(partial)*
- [ ] **BTS-198** Correct measured-vs-target units
- [ ] **BTS-199** Reviewable proposed fuel corrections
- [ ] **BTS-200** Compare suggested and current cell values
- [ ] **BTS-201** Accept/reject individual suggestions
- [ ] **BTS-202** Preview table smoothing
- [x] **BTS-203** No unattended automatic ECU writes
- [ ] **BTS-204** Versioned trace analysis report *(partial)*
- [x] **BTS-205** Export analyzed results
- [ ] **BTS-206** Tune before/after comparison *(partial)*
- [x] **BTS-207** Original sensor-value provenance

## 07. Diagnostics and Faults — P1

- [x] **BTS-208** Display actual ECU-reported faults only
- [x] **BTS-209** Never treat malformed protocol bytes as faults
- [x] **BTS-210** Distinct transport errors vs ECU faults
- [x] **BTS-211** Persistent confirmed-fault banner
- [x] **BTS-212** Clear only when ECU reports clear
- [ ] **BTS-213** Show original ECU fault text
- [ ] **BTS-214** Multiple independently reported fault records when available *(partial)*
- [x] **BTS-215** Conflict field A shortcut
- [x] **BTS-216** Conflict field B shortcut
- [ ] **BTS-217** Open exact loaded INI parameter
- [ ] **BTS-218** Show ECU pin labels *(partial)*
- [ ] **BTS-219** Trigger synchronization status *(partial)*
- [ ] **BTS-220** Trigger error history *(partial)*
- [ ] **BTS-221** Battery voltage status *(partial)*
- [ ] **BTS-222** 5 V reference status
- [ ] **BTS-223** Sensor validity indicators
- [ ] **BTS-224** Sensor range and wiring guidance
- [ ] **BTS-225** CAN interface status *(partial)*
- [ ] **BTS-226** Wideband controller status *(partial)*
- [ ] **BTS-227** Knock-system status *(partial)*
- [ ] **BTS-228** Connection fault timeline
- [ ] **BTS-229** Timestamp of fault observation *(partial)*
- [ ] **BTS-230** Fault occurrence counter where supported
- [ ] **BTS-231** Configuration change correlation
- [ ] **BTS-232** Export diagnostic report
- [ ] **BTS-233** Read-only fault history
- [ ] **BTS-234** Error-resolution verification *(partial)*
- [ ] **BTS-235** Display INI/ECU signature mismatch clearly
- [ ] **BTS-236** Power-cycle warning after affected calibration *(partial)*
- [ ] **BTS-237** Fault-screen navigation back to prior position

## 08. Configuration and INI Navigation — P1

- [ ] **BTS-238** Fully parsed INI menu hierarchy *(partial)*
- [x] **BTS-239** Folders before individual settings
- [x] **BTS-240** Semantic icons per subsystem
- [x] **BTS-241** Setting favorites
- [ ] **BTS-242** Recent settings list *(partial)*
- [ ] **BTS-243** Search by friendly label *(partial)*
- [ ] **BTS-244** Search by raw INI ID *(partial)*
- [ ] **BTS-245** Search by unit or pin function
- [ ] **BTS-246** Back stack and return scroll restoration *(partial)*
- [x] **BTS-247** Global quick jump to MAPS
- [ ] **BTS-248** Large numeric editor *(partial)*
- [ ] **BTS-249** Editable numeric step size *(partial)*
- [ ] **BTS-250** Valid numeric range display *(partial)*
- [x] **BTS-251** Enum option chooser
- [ ] **BTS-252** Boolean toggle where supported *(partial)*
- [x] **BTS-253** INI units and scaling
- [ ] **BTS-254** [PARTIAL] INI conditional fields *(partial)*
- [ ] **BTS-255** INI dynamic visibility
- [ ] **BTS-256** Contextual setting help
- [ ] **BTS-257** Display pin assignments *(partial)*
- [ ] **BTS-258** Detect overlapping pin assignments from ECU faults *(partial)*
- [x] **BTS-259** Highlight local pending changes
- [x] **BTS-260** Power-cycle-required marker
- [ ] **BTS-261** Tune-revision comparison
- [ ] **BTS-262** Deprecated parameter warning
- [ ] **BTS-263** Expose unsupported controls honestly
- [x] **BTS-264** Never fabricate unknown options
- [ ] **BTS-265** Remember map viewport per table *(partial)*
- [x] **BTS-266** Keyboard controls as alternative to touch
- [ ] **BTS-267** Font scaling and text accessibility *(partial)*
- [ ] **BTS-268** Local category history

## 09. Files and Tune Management — P2

- [ ] **BTS-269** [PARTIAL] Complete ECU tune download *(partial)*
- [ ] **BTS-270** SD calibration backup *(partial)*
- [ ] **BTS-271** Load and preview saved calibration *(partial)*
- [ ] **BTS-272** Compare two calibration versions *(partial)*
- [ ] **BTS-273** Changed parameter count *(partial)*
- [ ] **BTS-274** Changed map heatmap *(partial)*
- [ ] **BTS-275** Absolute and percent differences *(partial)*
- [ ] **BTS-276** Restore one parameter
- [ ] **BTS-277** Restore selected cells
- [x] **BTS-278** Prewrite full-page backup
- [ ] **BTS-279** Record backup timestamp and ECU identity *(partial)*
- [ ] **BTS-280** Name each tune revision
- [ ] **BTS-281** Revision description/notes
- [ ] **BTS-282** ECU firmware signature saved with tune *(partial)*
- [ ] **BTS-283** Associate logs with calibration revisions *(partial)*
- [x] **BTS-284** Differentiate SD snapshot from ECU active tune*
- [ ] **BTS-285** [PARTIAL] Recover interrupted local editing state *(partial)*
- [x] **BTS-286** Flash readback persistence verification
- [ ] **BTS-287** Import supported calibration formats
- [ ] **BTS-288** Export supported calibration formats
- [ ] **BTS-289** Tune history browser
- [ ] **BTS-290** Revision filter and search
- [ ] **BTS-291** SD storage health and free space
- [ ] **BTS-292** Configuration checksum comparison *(partial)*
- [ ] **BTS-293** Human-readable tune change report
- [ ] **BTS-294** Conflict resolution before a merge
- [ ] **BTS-295** Never silently overwrite tune backups

## 10. Bench and Developer Tools — P3

- [ ] **BTS-296** ECU identification and firmware report *(partial)*
- [ ] **BTS-297** INI compatibility inspector *(partial)*
- [ ] **BTS-298** Sensor calibration wizard
- [ ] **BTS-299** TPS calibration helper
- [ ] **BTS-300** Temperature sensor calibration
- [ ] **BTS-301** Analog channel scaling
- [ ] **BTS-302** CAN bus status
- [ ] **BTS-303** Input/output assignment report *(partial)*
- [ ] **BTS-304** Trigger pattern diagnostic graphs *(partial)*
- [ ] **BTS-305** Trigger waveform capture where exposed
- [ ] **BTS-306** Knock diagnostic data where exposed *(partial)*
- [ ] **BTS-307** Guarded ECU bench commands only in supported environment *(partial)*
- [ ] **BTS-308** Dedicated Stop for tests
- [ ] **BTS-309** Bench command timeout display *(partial)*
- [ ] **BTS-310** Show real ECU command acknowledgements *(partial)*
- [ ] **BTS-311** Export command trace
- [ ] **BTS-312** Error code descriptions
- [ ] **BTS-313** Network request profiler *(partial)*
- [ ] **BTS-314** Report ECU console features not available
- [ ] **BTS-315** Document hardware isolation test prerequisites

## 11. Connectivity and Performance — P0

- [ ] **BTS-316** Fast splash-to-live time *(partial)*
- [x] **BTS-317** Auto Wi-Fi reconnect
- [x] **BTS-318** Manual reconnect control
- [x] **BTS-319** Explicit disconnected state
- [x] **BTS-320** Confirmed ECU signature match
- [x] **BTS-321** Current firmware and cached INI version
- [x] **BTS-322** Real measured telemetry frequency
- [ ] **BTS-323** HTTP latency and operation duration *(partial)*
- [ ] **BTS-324** Dropped response counter
- [ ] **BTS-325** Configurable polling interval
- [x] **BTS-326** Separate GUI rendering and network thread
- [x] **BTS-327** Prioritize calibration reads/writes
- [ ] **BTS-328** Keepalive when supported
- [x] **BTS-329** Bounded request timeout
- [x] **BTS-330** Write uncertainty needs human verification
- [ ] **BTS-331** [PARTIAL] Read retry without repeating burns *(partial)*
- [x] **BTS-332** Field cache and map text cache
- [ ] **BTS-333** Efficient redraw of changed assets *(partial)*
- [ ] **BTS-334** Memory budget monitoring
- [x] **BTS-335** Log memory limits stated
- [ ] **BTS-336** Session state preserved across loss *(partial)*
- [ ] **BTS-337** Re-establish data freshness after reconnect *(partial)*
- [ ] **BTS-338** Never claim that 100 Hz tuning equals file throughput
- [ ] **BTS-339** Frame rate profiling on real original 3DS

## 12. Nintendo 3DS Interface — P0

- [x] **BTS-340** Upper screen instruments by default
- [x] **BTS-341** Lower screen navigation and controls
- [x] **BTS-342** Top screen dedicated map in edit mode
- [x] **BTS-343** Top screen dedicated graph in analysis mode
- [x] **BTS-344** Stylus large touch targets
- [x] **BTS-345** D-pad precise map selection
- [x] **BTS-346** Button-only alternative workflow
- [x] **BTS-347** L/R gauge cycling
- [ ] **BTS-348** Circle Pad navigation acceleration *(partial)*
- [ ] **BTS-349** Hold-to-repeat navigation
- [ ] **BTS-350** Customizable quick actions *(partial)*
- [x] **BTS-351** Native-size text and spacing
- [x] **BTS-352** Consistent dark contrast with rounded panels
- [ ] **BTS-353** Nonblocking short transitions *(partial)*
- [ ] **BTS-354** Resume after sleep *(partial)*
- [ ] **BTS-355** Connection refresh after wake *(partial)*
- [x] **BTS-356** Review logs while offline
- [ ] **BTS-357** Compare tunes offline *(partial)*
- [ ] **BTS-358** SD screenshot option
- [ ] **BTS-359** Optional stereo graph visualization only if useful *(partial)*
- [x] **BTS-360** Never obscure values behind decorations
- [x] **BTS-361** Scale font based on actual 400x240/320x240 displays
- [x] **BTS-362** No model viewer unrelated to tuning

## 13. Advanced Analysis and Reporting — P4

- [x] **BTS-363** Connected trace replay synchronization
- [x] **BTS-364** Map utilization frequency analysis
- [ ] **BTS-365** Fuel correction scatter plots
- [ ] **BTS-366** Sensor correlation chart
- [ ] **BTS-367** Per-cylinder diagnostic overlays
- [ ] **BTS-368** Derived channel expression engine
- [ ] **BTS-369** Calculated channels based on verifiable formulas
- [ ] **BTS-370** Selectable log event markers
- [ ] **BTS-371** Automated per-session summary *(partial)*
- [ ] **BTS-372** Workspace preset import/export
- [x] **BTS-373** Historical log comparison
- [ ] **BTS-374** Search within logs and revisions
- [ ] **BTS-375** Log sample quality score *(partial)*
- [ ] **BTS-376** Recommended next diagnostic checks based only on actual faults
- [x] **BTS-377** No imagined ECU status indicators
- [ ] **BTS-378** Share tuning workspace layouts
- [x] **BTS-379** Computer export without PC-required workflow
- [ ] **BTS-380** Portable JSON session metadata
- [ ] **BTS-381** Compare three or more saved tune versions
- [ ] **BTS-382** Session notes and attachable evidence *(partial)*

## Current release: v3.3

- Offline A/B comparison between two real SD log recordings: exact INI channel-name matching, relative time alignment, missing-data exclusion, plotted overlays, mean/absolute deltas, and a saved text report.
- Recorded-data quality mode: sample availability, largest actual time gap, repeated timestamps, and unchanged-value runs, with report export. These observations are not ECU fault diagnoses.
- ONE LARGE and TWO LARGE digital dashboard presets, plus per-gauge decimal precision saved on SD.
- Four-corner bilinear interpolation for a marked calibration rectangle, validated atomically and integrated with existing undo history.
- Per-field SD snapshots with exact ECU signature, INI name/page/offset/length, CRC32 and read-only comparison; **not** a complete ECU tune snapshot and no automatic ECU writes.
- Preserves v3.2's local map tools, log analysis, calibration backups, independent trigger bench rules, and diagnostics.

## Next releases (unimplemented)

- **v3.4:** Versioned full-calibration SD backups, offline tune revision browser and more complete comparison of axis and table values.
- **v3.5:** Broader log-filtering tools, scatter/XY analysis, session presets, and portable metadata.
- **Later:** Contextual INI help and hardware-tested performance/connection recovery. Features remain TODO until individually implemented.

## Required real-hardware checks

- [ ] Rebuild `.cia` successfully using the user's devkitPro environment.
- [ ] Test on original 3DS and New 3DS for layout and frame rate.
- [ ] Check log bookmarks survive application restart / SD remount.
- [ ] Verify Trace Mode with a real rusEFI log containing named X and Y channels.
- [ ] Verify readback and burn persistence with controlled bench data before production release.



## v3.3 development status

139 source-implemented, 101 partial, and 142 TODO (382 total).
See `FEATURE_TRACKER.csv`, `FEATURE_STATUS.md`, and `RELEASE_V33.md` for the precise scope of source-only checks.
No hardware verification was performed in this environment.
## v3.4 usability pass

The launcher, map action panel, replay tools, Dashboard actions, and cross-workspace back routes were reorganized for feature discovery. See `README_V34_WORKFLOW_NAVIGATION.md` and `FEATURE_TRACKER.csv`. This pass does not imply the remaining features are implemented or hardware verified.
