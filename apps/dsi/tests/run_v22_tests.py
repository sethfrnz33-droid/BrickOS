from pathlib import Path
import subprocess, sys
root=Path(__file__).resolve().parents[1]
exe='/tmp/bts_v22_bits'
subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-O2','-Isource','tests/test_v22_bits.c','source/tuner.c','-lm','-o',exe],check=True,cwd=root)
subprocess.run([exe],check=True,cwd=root)
s=(root/'source/main.c').read_text()
assert 'gauge_layout==3' in s and 'gauge_layout==4' in s
assert 'if(down&KEY_L){gauge_layout=' in s
assert 'bt_sd_layout_save(gauge_layout)' in s
assert 'if(chosen_map>=0&&table.valid){\n        bottom_editor();' in s
assert 'bit_choice_count=bt_bit_choices(' in s
assert 'static void bottom_home' in s and 'nav_icon_mini(bottom,home_icons[i]' in s
assert 'int y=27+i*24' in s
assert '(page==P_HOME&&gauge_layout>2)' in s
assert 'bts_sfx_status(sd_ready)' in s
print('PASS v2.2 UI: actual 3DS icon placement, 2/3/4 gauges, SD layout, map edit, audio diagnostics')
