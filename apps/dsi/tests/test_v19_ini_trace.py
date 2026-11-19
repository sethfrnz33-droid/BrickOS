from pathlib import Path
import re
r=Path(__file__).resolve().parents[1]
main=(r/'source/main.c').read_text()
tuner=(r/'source/tuner.c').read_text()
types=(r/'source/tuner.h').read_text()
for symbol in ('xchan[64], ychan[64]','map_live_trace','nearest_bin','LIVE TRACE',
               'nav_icon_mini(bottom,home_icons[i]','nav_icon_mini(top,category_icon',
               'static void bottom_sounds','P_SOUNDS','home_pick','compact_gauge('):
    assert symbol in main+types,(symbol,'missing')
assert 'cp(m->xchan,sizeof(m->xchan),a[1])' in tuner
assert 'cp(m->ychan,sizeof(m->ychan),a[1])' in tuner
assert 'nav_tabs();' not in main
assert 'if((down&KEY_UP)&&home_pick>0)' in main
assert 'if(down&KEY_R){gauge_layout=' in main
assert 'if(last_http_status==503)' in main
assert 'live_resume_frame=frame+6' in main
assert 'bts_sfx_play(sound_pick,sd_ready)' in main
assert 'BTS_ICON_SIZE 28' in (r/'source/ui_art.h').read_text()
assert 'bts_ui_font[95][14]' in (r/'source/ui_font.h').read_text()
assert len(list((r/'SD_Files/BrickECU/sfx').glob('*.pcm8')))==14
print('PASS: 3DS icon navigation, legible font, 2 gauges, 14 sounds, INI axis tracing, HTTP 503 pacing')
