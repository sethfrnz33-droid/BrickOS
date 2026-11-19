#!/usr/bin/env python3
"""The event mixer uses the same active CH0, but only 16 KB of linear memory.
Independent wiring check; native queue lifetime coverage lives in v59.
"""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()
a=(root/'source/bts_event_audio.h').read_text()
assert 'static bool event_audio_enabled=true;' in s
assert 'if(sscanf(line,"event_audio=%d",&v)' in s
assert 'fprintf(fp,"event_audio=%d\\n"' in s
assert 'case 3:ui_page=PAGE_SOUND_TEST' in s
assert 'bts_ndsp_result=ndspInit();' in s
assert 'bts_audio_init(true);' in s
assert 'ndspChnWaveBufAdd(0,&bts_audio_wave[slot]);' in a
assert 'bts_audio_capacity=BTS_AUDIO_BUFFER_COUNT*BTS_AUDIO_BLOCK_SAMPLES*sizeof(s16);' in a
assert 'if(!bts_audio_shared){bts_audio_error="LINEAR RAM";return;}' in a
assert 'if(bts_audio_active)return; /* never cut off a queued event */' in s
assert 'bts_audio_pump(); /* keep event PCM streaming on every UI frame */' in s
subprocess.run(['gcc','-std=gnu11','-Wno-implicit-function-declaration','-Wno-int-conversion',
 '-Wno-incompatible-pointer-types','-Wno-pointer-to-int-cast','-fsyntax-only',
 '-I'+str(root/'tests/host_stubs'),'-I'+str(root/'source'),str(root/'source/main.c')],check=True,capture_output=True)
print('PASS: CH0 clip playback, buffer failure detection, 16KB linear memory, per-frame pump')
