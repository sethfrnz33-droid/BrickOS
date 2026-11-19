# Brick Tuner Suite v5.3 — Host tests

- PASS — `test_v21_gauge_contract.py`
- PASS — `test_v21_trigger_ui.py`
- PASS — `test_v23_nav_graph.py`
- PASS — `test_v25_real_stim.py`
- PASS — `test_v26_faults.py`
- PASS — `test_v26_ini_autosync.py`
- PASS — `test_v30_tuning_ui.py`
- PASS — `test_v31_trace_undo.py`
- PASS — `test_v32_integration.py`
- PASS — `test_v33_integration.py`
- PASS — `test_v33_parser.py`
- PASS — `test_v34_build_fix.py`
- PASS — `test_v34_workspaces.py`
- PASS — `test_v35_tune_workflow.py`
- PASS — `test_v36_split_screen.py`
- PASS — `test_v37_tunerstudio_navigation.py`
- PASS — `test_v38_interaction.py`
- PASS — `test_v39_3d_unique_idle.py`
- PASS — `test_v41_real_surface_and_banner.py`
- PASS — `test_v42_banner_windows_python.py`
- PASS — `test_v43_dark_depth_surface.py`
- PASS — `test_v44_forward_depth_and_pitch.py`
- PASS — `test_v45_ui_and_burn.py`
- PASS — `test_v46_e46_banner.py`
- PASS — `test_v47_gutted_e46_banner.py`
- PASS — `test_v48_single_sided_banner.py`
- PASS — `test_v49_e46_diorama.py`
- PASS — `test_v51_donut_intro.py`
- PASS — `test_v51_intro_fx.py`
- PASS — `test_v52_ini_help_and_stereo.py`
- PASS — `test_v53_sfx_events.py`

Plus six independent native mocked ECU-write protocol scenarios and a local WAV conversion format check.

Real 3DS NDSP playback, audio volume, and native devkitARM build are unverified.

### Additional checks

- PASS: Native HTTP mock: 6/6 write/burn safety scenarios
- PASS: WAV converter produced 11/11 mono 22,050 Hz 16-bit PCM files from the provided inputs (test files discarded, not redistributed)
- PASS: 11 event rules, boost crossing and five-second coolant state machine
- PASS: No media files embedded in release; users copy prepared WAV files to their own 3DS SD card
