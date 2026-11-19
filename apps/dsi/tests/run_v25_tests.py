from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1]
exe='/tmp/bts-v25-ini'
subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-O2','-Isource','tests/test_v25_ini.c','source/tuner.c','-lm','-o',exe],cwd=root,check=True)
subprocess.run([exe]+sys.argv[1:],cwd=root,check=True)
subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-O2','-Itests/stubs','-Isource','-c','source/main.c','-o','/tmp/bts-v25-main.o'],cwd=root,check=True)
main=(root/'source/main.c').read_text()
assert 'if(down&KEY_START)break;' not in main
assert 'if(down&KEY_START){commit_map();return;}' in main
assert 'if(table.confirm_burn)' in main and 'if(down&KEY_A)commit_map();' in main
assert 'heat_color(' in main and 'tiny_fit(' in main
assert 'bt_ini_topic_help(' in main and 'editor_description' in main
assert 'resolve_active_formats' in main and 'read_mode_block' in main
assert 'next_curve_series' in main and 'refresh_graph_formulas' in main
assert 'virtual_z_map' in main and 'graph_list_formulas' in main
assert 'INI VIRTUAL AXIS / READ ONLY' in main
print('PASS v25 host main compile + START/Favorites/UI/trace/series regression')
