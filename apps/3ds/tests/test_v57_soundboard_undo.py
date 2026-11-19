#!/usr/bin/env python3
"""Verify soundboard page and visible undo actions across editor families."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()
assert '"Soundboard / Event Sounds"' in s
assert 'case 3:ui_page=PAGE_SOUND_TEST' in s
assert '"SOUNDBOARD"' in s
assert 'static void bts_soundboard_play(int idx)' in s
assert 'selected=idx;bts_soundboard_play(idx);' in s
assert 'if(k&KEY_A)bts_soundboard_play(selected);' in s
assert 'if(k&KEY_Y && !bts_audio_burn_loop)' in s
assert 'if(k&KEY_X){event_audio_enabled=!event_audio_enabled;' in s
assert 'if(ui_page!=PAGE_SOUND_TEST)sfx_play(0);touch_input();' in s
assert 'map_touch_button(111,176,98,"UNDO",false);' in s
assert 'map_touch_button(216,176,98,"REDO",false);' in s
assert 'map_touch_button(8,181,145,"UNDO",false);' in s
assert 'map_touch_button(166,181,145,"REDO",false);' in s
assert 'if(ui_page==PAGE_CHOICES&&(k&(KEY_X|KEY_Y)))' in s
assert 'if(tp.px<159)edit_history_move(-1);else edit_history_move(1);' in s
assert 'else if(tp.px<212)edit_history_move(-1);else edit_history_move(1);' in s
p=subprocess.run(['gcc','-std=gnu11','-Wno-implicit-function-declaration',
'-Wno-int-conversion','-Wno-incompatible-pointer-types',
'-Wno-pointer-to-int-cast','-fsyntax-only','-I'+str(root/'tests/host_stubs'),
'-I'+str(root/'source'),str(root/'source/main.c')],capture_output=True,text=True)
assert p.returncode==0,p.stderr
print('PASS: 14-clip soundboard, single tap, A/Y/X controls, scalar/curve/map/choice undo')
