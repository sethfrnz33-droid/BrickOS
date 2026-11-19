from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'source/main.c').read_text()
assert '#define SLOTS 2' in s
assert '{"AFRValue","VBatt"}' in s
assert 'BTS_LIVE_POLL_FRAMES=2' in s
assert 'BTS_SOCKET_GAP_FRAMES=1' in s
assert 'bool draw_now=frame%2==0||down;' in s
assert 'if(draw_now)present();' in s
assert 'measured_hz=live_count' in s
assert 'bts_sfx_play(bts_sound_for_entry' in s
assert 'bts_sfx_play(BTS_SOUND_CONNECTED' in s
sounds=list((r/'SD_Files/BrickECU/sfx').glob('*.pcm8'))
assert len(sounds)==14
assert all(32<=p.stat().st_size<=500000 for p in sounds)
assert 'bt_sd_asset_root()' in (r/'source/sfx.c').read_text()
assert 'soundEnable();' in s
print('PASS: two gauges, 30 Hz request pacing, real rate display, 14 SD audio clips')
