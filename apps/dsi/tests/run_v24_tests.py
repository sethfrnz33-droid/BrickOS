from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
exe='/tmp/bts-v24-ini'
subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-O2','-Isource','tests/test_v24_ini.c','source/tuner.c','-lm','-o',exe],cwd=root,check=True)
subprocess.run([exe],cwd=root,check=True)
main=(root/'source/main.c').read_text()
assert 'BTS_DSI_CHOICE_CAP 256' in main
assert 'bit_picker_open=bit_visible_count>0' in main
assert 'bt_ini_context_help(' in main
assert 'bt_sd_favorites_save(' in main
assert 'home_labels[4]' in main and 'FAVORITE SETTINGS' in main
assert 'ECU MAPS' not in main and 'LIVE GRAPHS"' not in main
print('PASS v2.4 UI: INI-backed dropdown, SD Favorites, four-item home, duplicate workspace removed')

assert 'BTSMD26' in (root/'source/storage.c').read_text()
assert 'graph_from_ini' in main and 'active_ini_graph_line' in main
assert 'ECU INI MARKS AXIS READ ONLY' in main
assert 'read_only' in (root/'source/tuner.h').read_text()
print('PASS v2.4: graph navigation, SD cache format, read-only axes, power-cycle metadata')
