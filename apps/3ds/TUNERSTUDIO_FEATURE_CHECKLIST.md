# Dream Tuner: TunerStudio parity checklist (v2.0; earlier feature inventory retained)

**Scope:** This is a practical feature inventory for TunerStudio MS Lite, MS (registered), MS Ultra and rusEFI-specific add-ons, not a claim that every edition has every feature. Exact availability depends on the ECU firmware, INI and licensed TunerStudio edition. Dream Tuner is **not** TunerStudio-complete; the statuses below distinguish feature availability from fully verified implementation.

**Status legend:** `[x]` means implemented in Dream Tuner source (not necessarily tested on original 3DS); `[-]` means partial; `[ ]` means not implemented. No simulator output control, automatic engine tuning, ECU safety bypasses, or live-engine write paths are introduced by v1.5.

## 1. Projects, connection and ECU identity

- [x] Associate with the saved `brick` Wi-Fi network (requires manual 3DS Internet settings).
- [x] Connect to BrickOS HTTP service at `192.168.4.1` and fetch ECU signature/status.
- [x] Download actual rusEFI INI and cache it to the SD card; avoid fake settings.
- [x] Detect missing/stale telemetry, display last known real samples.
- [-] Decode INI menus, calibrations, basic gauges, curves and table/axis data.
- [-] Auto-reconnect; distinguish disconnected/retrying and actual successful sample rates.
- [ ] Multi-ECU project wizard; multiple named profiles and configuration switching.
- [ ] Firmware version migration/reconciliation, changed signatures and incompatibility review.
- [ ] Network packet metrics, latency/throughput profiler and retry inspection tools.

## 2. Views, dashboards and presentation

- [x] **View Hub** on SELECT and as root `Views & Instruments` entry (v1.5).
- [x] Dashboard view (five layouts), full-screen top LCD, actual live data.
- [x] Assign nine dashboard slots to actual INI output channels.
- [x] Gauges use INI-defined ranges where resolved, rather than recent min/max.
- [x] Multi-channel time series (three dashboard channels plus INI-defined graph screens with up to six data channels; selectable window).
- [x] Knock Monitor with per-cylinder actual signal bars (v1.5).
- [x] Optional local UI navigation chimes via NDSP if DSP service is available (v1.5).
- [-] INI-defined graph views, larger numeric dashboards and custom typography (v1.7); layout editor remains limited to five predefined styles.
- [ ] Freeform dashboard designer: move/resize/add gauges via touchscreen.
- [ ] User-shared/loaded .dash assets and full dashboard theming tools.
- [ ] Configurable warning lights, alarms, thresholds and hysteresis per channel.
- [ ] Custom calculated channels; formulas and lookup tables from user projects.
- [x] Browse decoded INI output channels in a read-only, scrollable live inspector (v1.6).
- [ ] Histograms, scatter plots, XY plots, gauge grouping and activity indicators.
- [ ] Ultra designer-mode composite tuning/dyno views with multiple tables and gauges.
- [ ] Performance guarantee of 30+ actual received samples/second on original 3DS; benchmark pending.

## 3. ECU tuning and tables

- [x] Browse ECU menu categories, groups, named fields, maps and 1D curves from INI.
- [x] Context-aware top LCD: gauges normally, map when a map is opened.
- [x] 10×10 scrolling viewport of real numeric map cells with heatmap colors.
- [x] TunerStudio map direction: RPM low left, RPM high right; load low bottom, high top (when defined by the INI).
- [x] Show real axis values; support basic X/Y axis editing when the INI allows.
- [x] Trace current RPM/load into map cells if channels/axis values are available.
- [-] Scalar/bitfield choices, option labels, staged local calibration edits.
- [-] ECU read/write/burn/verify path: present in inherited source, **bench-only and not fully hardware-validated**; firmware interlocks preserved.
- [-] Detect some INI `requiresPowerCycle` rules and report on verified changes.
- [ ] Complete INI dialog engine, conditional visibility, nested panels and every standard button/action.
- [ ] Undo/redo queue across multiple fields; simultaneous dirty-cell comparison.
- [ ] Multi-cell select, smooth interpolation, flood fill, table math/scaling, smoothing.
- [ ] Table generators, baseline/overlay display, 2D-to-3D surface switcher.
- [ ] Color palettes/settings per table, persistent individual map zooms.
- [ ] Complete project/tune save, load, restore and MSQ compatibility.
- [ ] Offline tune editing without a running ECU plus full diff and merge.
- [ ] Safe/verified ECU write transaction rollback and end-to-end acceptance testing.

## 4. Logging, playback and analysis

- [x] Live nine-channel telemetry RAM recorder with CSV export on SD.
- [x] Live history graphs based on real telemetry; no manufactured readings.
- [-] Files export and simple status; limited channels/samples, no playback UI.
- [ ] Rich datalog channel/trigger selection, pretrigger and timed recordings.
- [ ] SD-card ECU log list/download in the handheld UI.
- [-] On-device playback for Dream Tuner nine-channel CSV files, 1x/2x/4x playback, touchscreen timeline seek and 3-channel history; synchronized ECU map traces/MLG playback remain unavailable (v1.6).
- [ ] Log filtering, annotations, computed fields and CSV/MLG import.
- [-] Recorded per-channel min/mean/max, valid-sample counts and normalized 3-channel histories; histograms, XY scatter and full MegaLogViewer-style analysis not available (v1.6).
- [ ] Gauge and table overlays compared to a recorded engine session.

## 5. Tuning aids and engine analysis

- [x] v2.0: Offline 720-degree crank/cam missing-tooth waveform visualizer, adjustable **local preview** RPM and wheel preset. **No ECU actuation.**
- [x] v2.0: Read-only live RPM, trigger error count and sync-gap-ratio monitor when corresponding named INI output channels exist.
- [ ] Actual remote trigger START remains unsupported; v2.2 can send a fixed ECU trigger STOP command through BrickOS v4.6, but ECU running state is unverified. Remote activation can energize engine-control hardware.


- [-] Knock sensor levels/cylinder monitor and knock-retard/count visualization from standard INI channels (v1.5).
- [ ] rusEFI knock **frequency spectrogram**. This is provided by an additional rusEFI TunerStudio plugin and needs its packet format + an ESP32 forwarding/decoder endpoint; a level bar is not a replacement.
- [ ] Knock waveform capture and replay/threshold inspection.
- [ ] Trigger/composite/tooth/sync loggers, missing-tooth analysis.
- [ ] VE Analyze Live / automatic corrections and filters.
- [ ] AFR target comparison and closed-loop fuel correction visualizers.
- [ ] Acceleration enrichment analyzer and tuning wizards.
- [ ] Injector dead-time test/calibration wizard.
- [ ] Dyno calculation/traction/perfect-run views and model fitting.
- [ ] Ultra individual-cylinder trim auto-tune.
- [ ] SD-card logger configuration wizard, board-specific IO diagnostics.

## 6. Diagnostics and control

- [x] Wi-Fi/ECU status page, real ESP status where returned, last HTTP error.
- [-] INI action/menu resolution: 52 of 304 actionable menus unresolved with supplied fixture (v1.4 baseline).
- [ ] Full diagnostic trouble code decode, clear and event history.
- [ ] ECU console commands and firmware upgrades.
- [ ] CAN monitor/dashboard and bus analysis view.
- [ ] Simulator and physical hardware output commands: deliberately **not enabled** in Dream Tuner; no hardware actuation from the handheld.

## 7. Navigation, accessibility and user experience

- [x] Original Nintendo 3DS dual-screen navigation; D-pad/Circle Pad buttons.
- [x] Search through named INI menus and maps; saved favourites.
- [x] Context switch between maps/gauges, MAP editor on top display.
- [x] View Hub with explicit destinations, independent of INI availability (v1.5).
- [-] Menu font scaling and scrolling; continued readability improvements needed.
- [-] Rustic original sprite artwork and earth-tone UI; not at target polished-game quality.
- [ ] User adjustable font sizes, skin packs, language/localization, accessibility modes.
- [ ] Complete controller mapping editor, keyboard macros and shortcuts.
- [ ] Tutorial/help per setting from rusEFI docs, INI field documentation.
- [ ] On-device test suite and stable feature-level releases.

## v1.6 additions and remaining gaps

- [x] Saved CSV file browser and offline playback with time scrubbing.
- [x] Recorded-session min/mean/max statistics and valid-sample counts.
- [x] INI-based live output-channel inspector (read-only).
- [ ] On-hardware performance tests of replay, SD speed and navigation.

## Highest-priority acceptance criteria

1. No blank or nonfunctional `View` menu. Every row opens its advertised page. Knock page shows real values or `--`.
2. 30 FPS UI target remains responsive while telemetry is slow or disconnected; report actual RX Hz and frame timings separately.
3. All INI menu targets resolve with correct labels, conditional rules, controls and readback; no invented values.
4. Read-only knock data works as above; frequency spectrogram requires explicit documented data protocol.
5. Safe, verified restore/diff workflow exists before considering full calibration transaction parity.
6. On hardware, test the fresh CIA, not merely native host syntax.

## Sources

- TunerStudio MS feature comparison and official feature descriptions: https://www.tunerstudio.com/index.php?Itemid=59&id=54&option=com_content&view=article
- Official Ultra Tuning/Dyno Views: https://tunerstudio.com/index.php/products/tuner-studio/tsarticles/114-tunerstudio-tuning-and-dyno-views
- Official Ultra overview: https://tunerstudio.com/index.php/products/tuner-studio/tsarticles/120-introducing-tunerstudio-ms-ultra
- rusEFI knock-sensing documentation: https://wiki.rusefi.com/knock-sensing/
- rusEFI diagnostics and loggers: https://wiki.rusefi.com/Diagnostics-and-Logging/
- devkitPro NDSP API: https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/ndsp/ndsp.h
