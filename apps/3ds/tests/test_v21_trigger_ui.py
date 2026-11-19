from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()
header=(root/'source/trigger_preview.h').read_text()
checks={
    'INI trigger buttons do not falsely open offline viewer': 'warn_before_hazardous_test(BRICK_TEST_TRIGGER);' in s,
    'separate offline page reachable from views': '"Trigger Waveform (Offline)"' in s and 'else if(which==11)open_offline_trigger(false);' in s,
    'GUI uses offline generated crank and cam': 'dream_crank_high(angle,wheel)' in s and 'dream_cam_high(angle)' in s,
    'trace is animated via local clock': 'stim_phase(osGetTime())' in s,
    'user can pause and adjust RPM and wheel': 'stim_preview=!stim_preview;' in s and 'dream_clamp_rpm(stim_rpm+100)' in s and 'stim_wheel+1' in s,
    'separate ECU readings, not simulator values': 'runtime_value("RPMValue",&live_rpm)' in s and 'runtime_value("totalTriggerErrorCounter",&errors)' in s,
    'extended telemetry requested only for view': 'extend_live_for_trigger();' in s and 'live_length=base_live_length;ui_page=stim_return_page;' in s,
    'real ECU command gated by server': 'post_api("/api/stim/start"' in s and 'post_ts("Z"' not in s,
    'no electrical output methods in model': 'gpio' not in header.lower() and 'socket' not in header.lower() and 'TS_CRC' not in header,
    'no false ECU-stimulator activation message': 'LOCAL stimulator preview ON; ECU untouched' not in s,
    'new diagnostic build number': 'DS v3.4' in s,
}
for label,ok in checks.items(): print(('PASS' if ok else 'FAIL')+': '+label)
assert all(checks.values()),'Trigger UI integration mismatch'
