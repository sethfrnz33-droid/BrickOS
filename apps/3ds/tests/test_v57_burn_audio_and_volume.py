#!/usr/bin/env python3
"""Burn loop state must stop before XP/failure cue; PCM volumes remain consistent."""
from pathlib import Path
import numpy as np
import subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text();a=(root/'source/bts_event_audio.h').read_text()
assert 'bts_audio_start_burn_loop(event_audio_enabled);' in s
assert 'if(k&KEY_B){write_queued=false;bts_audio_stop_burn_loop();' in s
assert 'case TASK_WRITE:' in s
assert 'bts_audio_stop_burn_loop();' in s
assert 'if(result>0&&write_verified)bts_audio_play(BTS_SOUND_BURN_OK,event_audio_enabled);' in s
assert 'else bts_audio_play(BTS_SOUND_CONNECTED,event_audio_enabled); /* fart with reverb */' in s
assert 'if(bts_audio_burn_loop){bts_audio_error="BURN AUDIO BUSY";return false;}' in a
assert 'if(!bts_audio_burn_loop)return false;' in a
assert 'bts_audio_stop_burn_loop(void)' in a
for p in sorted((root/'art/event_audio').glob('*.pcm')):
    x=np.fromfile(p,dtype='<i2').astype(np.float64)/32768
    assert x.size>0 and np.max(np.abs(x))<=.941
    assert x.size>10
    level=np.sqrt(np.mean(x**2))
    assert .24<level<.30,(p,level)
assert len(list((root/'art/event_audio').glob('*.pcm')))==14
subprocess.run(['python3',str(root/'tests/test_v59_streaming_audio.py')],check=True,capture_output=True)
print('PASS: burn loop, verified/failure cues, and all 14 louder matched PCM loudness levels')
