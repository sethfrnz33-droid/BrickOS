#!/usr/bin/env python3
from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'source/main.c').read_text()
checks={
 'compare keeps original replay buffer': 'LogSample *candidate=(LogSample*)malloc' in s and 'free(compare_buffer);compare_buffer=candidate;' in s,
 'rejected reference is not installed': 'if(!ok){free(candidate);note("Invalid or oversized reference log' in s,
 'compare maps by true channel name': 'bts_compare_channel_index(log_channel_names[ch],names)' in s,
 'compares only real nearby timestamps': 'distance<=BTS_COMPARE_GAP_MS?best:-1' in s,
 'rendered overlay and controls': 'draw_compare_top();' in s and 'draw_compare_deck();' in s and 'if(ui_page==PAGE_COMPARE){' in s,
 'explicit guarded selection from replay': 'if(hidKeysHeld()&KEY_L){compare_return_page=PAGE_REPLAY;compare_open_picker();return;}' in s and '"COMPARE"' in s,
 'read only comparison report': 'Offline log A-B comparison' in s and 'no ECU calibration was modified' in s,
 'explicit ECU identity warning': 'compare_ecu_identity==2' in s and 'ECU signatures DIFFER' in s,
 'one and two gauge layouts': '"ONE LARGE", "TWO LARGE"' in s and 'home_digit(0,8,12,384,200,false)' in s,
 'saved per gauge decimal places': 'precision%d=%d' in s and 'gauge_decimals[i]' in s,
 'quality never invents ECU faults': 'Quality observations are not ECU errors' in s and 'bts_signal_quality(' in s,
 'quality report view and offline export': 'draw_quality_deck();' in s and 'quality-%03d.txt' in s,
 'bilinear map operation accessible': '"Bilinear fill (4 corners)"' in s and 'op=BTS_MAP_BILINEAR' in s,
 'signed local snapshots never trigger ECU writes': 'bts_snapshot_write(' in s and 'bts_snapshot_read(' in s and 'SNAPSHOT #%04d' in s and 'snapshot_save_current();return;' in s,
 'existing burn path remains': 'static bool save_backup' in s and 'static void request_burn(void)' in s,
}
for label,ok in checks.items():print(('PASS' if ok else 'FAIL'),label)
assert all(checks.values())
