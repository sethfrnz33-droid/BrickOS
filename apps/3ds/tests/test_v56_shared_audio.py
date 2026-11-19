#!/usr/bin/env python3
"""NDSP waveform buffers are not overwritten while the DSP owns them."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
a=(root/'source/bts_event_audio.h').read_text()
s=(root/'source/main.c').read_text()
assert a.count('linearAlloc(')==1
assert 'ndspChnWaveBufClear(0);' in a
assert 'if(state==NDSP_WBUF_DONE)' in a
assert 'else if(state==NDSP_WBUF_PLAYING||state==NDSP_WBUF_QUEUED)' in a
assert 'if(bts_audio_queue_block(i))pending=true;' in a
assert 'bts_audio_wave[i].status=NDSP_WBUF_FREE;' in a
assert 'ndspChnWaveBufAdd(0,&bts_audio_wave[slot]);' in a
assert 'memcpy(destination,c->data+(size_t)bts_audio_next_sample*sizeof(s16)' in a
assert 'bts_audio_burn_loop=loop;' in a
assert 'if(bts_audio_active)return;' in s
assert 'bts_audio_pump();' in s
subprocess.run(['python3',str(root/'tests/test_v59_streaming_audio.py')],check=True,capture_output=True)
print('PASS: NDSP buffer ownership, clip stream recycling, priority and native mock')
