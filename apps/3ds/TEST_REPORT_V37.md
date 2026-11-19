# Brick Tuner Suite v3.7 — Host Test Report

Run against the versioned project source on 2026-11-17 (PST).

- 16 / 16 Python regression scripts passed: `test_v21_gauge_contract.py`, `test_v21_trigger_ui.py`, `test_v23_nav_graph.py`, `test_v25_real_stim.py`, `test_v26_faults.py`, `test_v26_ini_autosync.py`, `test_v30_tuning_ui.py`, `test_v31_trace_undo.py`, `test_v32_integration.py`, `test_v33_integration.py`, `test_v33_parser.py`, `test_v34_build_fix.py`, `test_v34_workspaces.py`, `test_v35_tune_workflow.py`, `test_v36_split_screen.py`, `test_v37_tunerstudio_navigation.py`.
- Full source host C syntax check passed (`gcc -std=gnu11` with 3DS host stubs).
- The new compiled navigation exercise validated top highlight -> preview lower -> A focuses lower -> A enters real VE, group child navigation, and no synthetic all-map entries.
- The menu order test now follows the ECU INI sequence, rather than moving all folders above normal settings.
- The new icon sheet includes 13 exact screenshot-derived PNG pictograms and is included in the build manifest.
- This system does **not** have a devkitARM + `tex3ds` 3DS build environment. The generated CIA and real-console results are therefore **unverified**. Host mocks cannot validate GPU textures, frame-rate or ECU wiring.
