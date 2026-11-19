#!/usr/bin/env python3
"""v3.2 full-code integration contracts; pure math is tested separately in C."""
from pathlib import Path
s=(Path(__file__).resolve().parents[1]/'source/main.c').read_text()
checks={
 'map tools menu has at least 15 real operations': '#define MAP_TOOL_COUNT 18' in s and 'static void map_tools_apply(void)' in s,
 'map tools reachable by touch': 'map_tools_open=true;map_tool_cursor=map_tool_scroll=0;' in s,
 'map tools keyboard and cancel': 'if(map_tools_open){' in s and 'if(k&KEY_B){map_tools_open=false;return;}' in s,
 'all region operations isolated from real ECU': 'static uint8_t candidate[EDIT_CAP]' in s and 'memcpy(editor,candidate,(size_t)editor_length)' in s,
 'numeric out-of-range refuses entire edit': 'INI limit/encoding rejected a cell; entire edit cancelled' in s,
 'range edits saved as single undo history frame': 'edit_history_capture(&edit_undo_before);\n memcpy(editor,candidate' in s,
 'baseline differences outlined on map': 'if((map_diff_mode||snapshot_diff_mode)&&editor_loaded)' in s and 'stroke_cell(x+3,y+3,cw-6,ch-6,orange);' in s,
 'log replay opens analysis with start': 'if(k&KEY_START){analysis_return_page=PAGE_REPLAY;insights_open();return;}' in s and '"ANALYZE"' in s,
 'analysis has 9-channel and timescope controls': 'insight_channel=(insight_channel+1)%LOG_FIELDS' in s and 'insight_window=(insight_window+1)%4' in s,
 'analysis renders upper and lower screen': 'draw_insights_top();' in s and 'draw_insights_deck();' in s,
 'analysis is offline and read-only': 'Read-only log analysis' in s and 'No recorded samples beyond the selected percentiles' in s,
 'events point to actual recorded samples': 'replay_seek(replay_buffer[found].t_ms)' in s,
 'analysis uniquely numbered file export': 'analysis-%03d.txt' in s and 'log_basename_valid(replay_filename)' in s,
 'dashboard peak hold and running means': 'gauge_stats_accept(i,(float)v)' in s and 'GaugeCapture' in s,
 'gauge reset and mode buttons': 'if(k&KEY_Y){gauge_stat_view=' in s and 'if(k&KEY_X){for(int i=0;i<GAUGE_SLOTS;i++)gauge_stats_reset(i);' in s,
 'recorded session carries ECU and INI identity': 'record_ecu_signature' in s and 'record_ini_signature' in s and '.session.txt' in s,
 'no unsafe writes added to analysis paths': 'insights_export' in s and 'post_ts("B",hex)' in s,
 'old ECU burn tests still present': 'request_burn(void)' in s,
 '3ds source includes complete benchmarked math modules': '#include "bts_map_ops.h"' in s and '#include "bts_log_insights.h"' in s,
 'no 3d model viewer reintroduced': '#include "pikmin_scene.h"' not in s and 'pikmin_scene.c' not in s,
}
for label,ok in checks.items():print(('PASS ' if ok else 'FAIL ')+label)
assert all(checks.values())
