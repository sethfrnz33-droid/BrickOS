#!/usr/bin/env python3
"""DSi v26: full INI label and UI-flow regression (host-only)."""
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
ini = Path(sys.argv[1]).resolve() if len(sys.argv)>1 else None
if not ini or not ini.is_file():
    raise SystemExit('Usage: python tests/run_v26_tests.py /path/to/rusefi_proteus_f7.ini')
exe='/tmp/bts-v26-ini'
subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-O2','-Isource',
    'tests/test_v26_ini.c','source/tuner.c','-lm','-o',exe],cwd=root,check=True)
subprocess.run([exe,str(ini)],cwd=root,check=True)
gauge_exe='/tmp/bts-v26-gauges'
subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-O2','-Isource',
    'tests/test_v26_gauges.c','source/tuner.c','-lm','-o',gauge_exe],cwd=root,check=True)
subprocess.run([gauge_exe,str(ini)],cwd=root,check=True)
snapshot_exe='/tmp/bts-v26-gauge-snapshot'
subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-O2','-Isource',
    'tests/test_v26_gauge_snapshot.c','source/tuner.c','source/storage.c',
    'source/signature.h','-lm','-o',snapshot_exe],cwd=root,check=True)
subprocess.run([snapshot_exe,str(ini)],cwd=root,check=True)
subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-O2','-Itests/stubs',
    '-Isource','-c','source/main.c','-o','/tmp/bts-v26-main.o'],cwd=root,check=True)
main=(root/'source/main.c').read_text()
store=(root/'source/storage.c').read_text()
parser=(root/'source/tuner.c').read_text()
assert 'BTSMD34' in store and 'head.format=5' in store
assert 'add_ref(d,d->current_dialog,a[0],a[0],1' in parser
assert 'panel_layout_wrapper' in main and 'dialog_entry_find' in main
assert 'gauge_only_panel' in main and 'rebuild_dialog_gauges' in main
assert 'draw_ref_gauge(' in main and 'dialog_gauge_expr_valid' in main
assert 'gauge_count' in (root/'source/tuner.h').read_text()
assert 'bit_visible_ids[bit_picker_pick]' in main
assert '"(INI RESERVED)"' not in main and 'INI RESERVED / CANNOT SELECT' not in main
assert 'if(table.confirm_burn){\n            if(down&KEY_A)commit_map();' in main
assert 'else if(down&KEY_B){table.confirm_burn=false;' in main
assert 'if(down&KEY_START){commit_map();return;}' in main
assert 'if(down&KEY_R)favorite_current_editor();' in main
assert 'if(down&KEY_R)commit_map();' not in main
assert 'bottom_burn_confirm' in main
assert 'PRESS A TO BURN' in main
print('PASS v26 compiled UI + INI reference gauges / placement / hidden reserved / START-A burn / R Favorite')
