from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()
mk=(root/'Makefile').read_text()
sh=(root/'build-cia.sh').read_text()
checks={
 'main app title': 'BRICK TUNER SUITE' in s,
 'Home Menu title': 'APP_TITLE := Brick Tuner Suite' in mk,
 'CIA metadata title': 'Brick Tuner Suite' in sh,
 'new DS diagnostic version': 'DS v3.4' in s,
 'retains last valid each packet': 'gauge_hold_accept(&view_values[i],&view_has[i],v,true)' in s and 'gauge_hold_accept(&displayed[i],&has_value[i],value,true)' in s,
 'long outage never clears gauge': 'now-live_received_at>12000' not in s and 'live_stale=live_valid && gauge_hold_is_stale' in s,
 'shows held sample instead of live': ('DATA HELD' in s or 'RX HELD' in s),
 'channel reassignment invalidates old value': 'gauge_hold_reset(&view_has[gauge_pick_slot])' in s,
 'cached settings retained': '#define DISPLAY_CFG "sdmc:/BrickECU/dreamtuner.cfg"' in s,
 'legacy logs remain visible': 'strncmp(name,"dreamtuner-",11)' in s,
 'new logs branded': 'logs/bricktuner-%04d.csv' in s,
 'live ECU stim guarded by preflight': 'STIM_PREPARE' in s and 'warn_before_hazardous_test(BRICK_TEST_TRIGGER);' in s and 'TASK_STIM_START' in s,
}
for label,ok in checks.items():print(('PASS' if ok else 'FAIL')+': '+label)
assert all(checks.values())
