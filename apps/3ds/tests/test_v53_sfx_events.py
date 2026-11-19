#!/usr/bin/env python3
"""Verify the shipped event rules, actual C integration, and embedded audio contract.
The 3DS DSP itself requires hardware verification.
"""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()
backend=(root/'source/bts_event_audio.h').read_text()
assert 'bts_audio_init(true);' in s
assert 'bts_audio_close();' in s
assert 'bts_context_named(label,target);' in s
assert 'bts_audio_play(BTS_SOUND_CONFIG_ERROR,event_audio_enabled)' in s
assert 'bts_audio_play(BTS_SOUND_BURN_OK,event_audio_enabled)' in s
assert 'if(result>0&&write_verified)' in s
assert 'bts_audio_start_burn_loop(event_audio_enabled);' in s
assert 'bts_audio_stop_burn_loop();' in s
assert 'bts_audio_burn_loop=loop;' in backend
assert 'bts_audio_play(BTS_SOUND_CONNECTED,event_audio_enabled); /* fart with reverb */' in s
assert 'bts_audio_play(BTS_SOUND_CONNECTED,event_audio_enabled)' in s
assert 'bts_audio_play(BTS_SOUND_UNSAVED,event_audio_enabled)' in s
assert 'bts_level_cross(&bts_boost_level,v,100.0,98.0)' in s
assert 'bts_level_repeat(&bts_coolant_level,v,105.0,now,5000)' in s
assert 'now-live_received_at>900' in s
assert 'bts_sound_for_change(f->name,choices[selected].label,old_choice,(double)chosen)' in s
assert 'static bool bts_sound_runtime' in s
assert 'DSP_FlushDataCache(destination,BTS_AUDIO_BLOCK_SAMPLES*sizeof(s16));' in backend
assert 'ndspChnWaveBufAdd(0,&bts_audio_wave[slot])' in backend
assert 'bts_load_embedded' in backend
assert 'fopen(path,"rb")' not in backend
assert 'BTS_SFX_DIR' not in backend
assert 'BTS_AUDIO_MAX_SAMPLES' in backend
native='''
#include <stdio.h>
#include <assert.h>
#include "bts_audio_rules.h"
int main(void){
 assert(bts_sound_for_entry("Launch Control","")==BTS_SOUND_LAUNCH);
 assert(bts_sound_for_entry("VVL Control","")==BTS_SOUND_VVL);
 assert(bts_sound_for_entry("Nitrous Control","")==BTS_SOUND_PRESSURE);
 assert(bts_sound_for_entry("Rotational Idle","")==BTS_SOUND_PRESSURE);
 assert(bts_sound_for_entry("Ignition Limiter","")==BTS_SOUND_FIREWORK);
 assert(bts_sound_for_entry("Anti-Lag ALS","")==BTS_SOUND_FIREWORK);
 assert(bts_sound_for_entry("Fuel Setup","")==-1);
 assert(bts_sound_for_choice("cylindersCount","4 Cylinders",4)==BTS_SOUND_GONG);
 assert(bts_sound_for_choice("cylindersCount","6 Cylinders",6)==-1);
 assert(bts_sound_for_choice("triggerType","Honda K24",34)==BTS_SOUND_GONG);
 assert(bts_sound_for_choice("rpmHardLimit","Honda",4000)==-1);
 assert(bts_sound_for_change("cylindersCount","",6,8)==BTS_SOUND_CYLINDER_OVER6);
 BtsAudioLevel p={0},c={0};
 assert(!bts_level_cross(&p,90,100,98));
 assert(bts_level_cross(&p,101,100,98));
 assert(!bts_level_cross(&p,110,100,98));
 assert(!bts_level_cross(&p,99,100,98));
 assert(!bts_level_cross(&p,97,100,98));
 assert(bts_level_cross(&p,102,100,98));
 assert(!bts_level_repeat(&c,104,105,1000,5000));
 assert(bts_level_repeat(&c,105.1,105,1100,5000));
 assert(!bts_level_repeat(&c,110,105,6099,5000));
 assert(bts_level_repeat(&c,110,105,6100,5000));
 assert(!bts_level_repeat(&c,105,105,6200,5000));
 assert(bts_level_repeat(&c,110,105,6300,5000));
 puts("PASS: all cue rules, strict crossing and five-second coolant re-arm");
}
'''
with tempfile.TemporaryDirectory() as td:
    p=Path(td)/'events.c';p.write_text(native)
    binary=Path(td)/'events'
    subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-Isource',str(p),'-o',str(binary)],cwd=root,check=True)
    subprocess.run([str(binary)],check=True)
subprocess.run(['gcc','-std=gnu11','-Wno-implicit-function-declaration','-Wno-int-conversion',
 '-Wno-incompatible-pointer-types','-Wno-pointer-to-int-cast','-fsyntax-only',
 '-I'+str(root/'tests/host_stubs'),'-I'+str(root/'source'),str(root/'source/main.c')],check=True)
print('PASS: source wiring + native C event rules + host syntax')
