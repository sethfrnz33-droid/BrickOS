# Brick Tuner Suite v3.8 — Host Test Report

Tested against the release folder on 2026-11-18. Actual New Nintendo 3DS/3DS XL rendering, devkitARM compilation, and connected ECU testing were not available.

```text
PASS test_v21_gauge_contract.py
PASS test_v21_trigger_ui.py
PASS test_v23_nav_graph.py
PASS test_v25_real_stim.py
PASS test_v26_faults.py
PASS test_v26_ini_autosync.py
PASS test_v30_tuning_ui.py
PASS test_v31_trace_undo.py
PASS test_v32_integration.py
PASS test_v33_integration.py
PASS test_v33_parser.py
PASS test_v34_build_fix.py
PASS test_v34_workspaces.py
PASS test_v35_tune_workflow.py
PASS test_v36_split_screen.py
PASS test_v37_tunerstudio_navigation.py
PASS test_v38_interaction.py
```

**Result:** 17/17 host Python regression scripts passed. The source passes host GCC syntax checks with 3DS API stubs. The host compiler cannot validate graphics rendering or devkitARM performance, nor prove the correctness of ECU responses under all circumstances.

Risk controls: existing ECU RPM/backup/write/burn checks unchanged. No ECU write path exists in 3D viewing mode.
