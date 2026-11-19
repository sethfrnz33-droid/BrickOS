#!/usr/bin/env python3
"""Exercise the full 14-clip PCM stream against simulated NDSP buffer statuses.
The mock advances NDSP statuses; it does not claim actual 3DS speaker output.
"""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
main=(root/'source/main.c').read_text()
audio=(root/'source/bts_event_audio.h').read_text()
assert 'bts_audio_pump(); /* keep event PCM streaming on every UI frame */' in main
assert 'if(bts_audio_active)return; /* never cut off a queued event */' in main
assert 'bts_audio_play(BTS_SOUND_UNDO,event_audio_enabled)' in main
assert 'CUE %d: %s' in main
assert 'bts_audio_stop();sound_test_queued=false;' in main
assert 'BTS_AUDIO_BLOCK_SAMPLES 4096u' in audio
assert 'NDSP_WBUF_DONE' in audio
assert 'bts_audio_next_sample' in audio

native=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int16_t s16; typedef uint32_t u32; typedef uint8_t u8;
enum{NDSP_FORMAT_MONO_PCM16=1,NDSP_INTERP_LINEAR=1,
     NDSP_WBUF_FREE=0,NDSP_WBUF_QUEUED=1,NDSP_WBUF_PLAYING=2,NDSP_WBUF_DONE=3};
typedef struct wave {s16 *data_pcm16;u32 nsamples;int status;bool looping;struct wave *next;} ndspWaveBuf;
static int allocation_count=0, queued_count=0, clear_count=0;
static size_t allocated=0;
static void *linearAlloc(size_t n){allocated=n;allocation_count++;return aligned_alloc(128,(n+127)&~(size_t)127);}
static void linearFree(void *p){free(p);}
static void DSP_FlushDataCache(void *p,size_t n){assert(p&&n>0&&n<=8192);}
static void ndspChnSetFormat(int ch,int fmt){assert(ch==0&&fmt==NDSP_FORMAT_MONO_PCM16);}
static void ndspChnSetRate(int ch,float rate){assert(ch==0&&rate==22050.0f);}
static void ndspChnSetInterp(int ch,int value){assert(ch==0&&value==NDSP_INTERP_LINEAR);}
static void ndspChnSetMix(int ch,float v[12]){assert(ch==0&&v[0]>.8f&&v[1]>.8f);}
static void ndspChnWaveBufClear(int ch){assert(ch==0);clear_count++;}
static void ndspChnWaveBufAdd(int ch,ndspWaveBuf *w){
 assert(ch==0&&w&&w->data_pcm16&&w->nsamples>0&&w->nsamples<=4096);
 assert(((uintptr_t)w->data_pcm16 % 128)==0);
 w->status=NDSP_WBUF_QUEUED;queued_count++;
}
#include "bts_event_audio.h"
static void advance_to_end(void){
 int guard=10000;
 while(bts_audio_active && --guard>0){
    for(int i=0;i<BTS_AUDIO_BUFFER_COUNT;i++)
      if(bts_audio_wave[i].status==NDSP_WBUF_QUEUED)
        bts_audio_wave[i].status=NDSP_WBUF_DONE;
    bts_audio_pump();
 }
 assert(guard>0);
}
int main(void){
 bts_audio_init(true);
 assert(bts_audio_ready && bts_audio_loaded==14);
 assert(allocation_count==1 && allocated==16384);
 for(int cue=0;cue<BTS_SOUND_COUNT;cue++){
    u32 needed=bts_audio_clips[cue].count;
    assert(needed>0);
    assert(bts_audio_play(cue,true));
    assert(bts_audio_now_playing==cue&&bts_audio_active);
    assert(bts_audio_last_cue==cue&&bts_audio_last_ok);
    assert(bts_audio_wave[0].nsamples>0);
    assert(memcmp(bts_audio_wave[0].data_pcm16,bts_audio_clips[cue].data,
                  (size_t)bts_audio_wave[0].nsamples*2)==0);
    advance_to_end();
    assert(!bts_audio_active && bts_audio_now_playing==-1);
 }
 assert(queued_count>100);
 assert(bts_audio_start_burn_loop(true));
 assert(bts_audio_burn_loop);
 for(int n=0;n<100;n++){
   for(int i=0;i<2;i++)if(bts_audio_wave[i].status==NDSP_WBUF_QUEUED)
      bts_audio_wave[i].status=NDSP_WBUF_DONE;
   bts_audio_pump();
   assert(bts_audio_active);
 }
 assert(!bts_audio_play(BTS_SOUND_UNDO,true)); // burn has priority
 bts_audio_stop_burn_loop();
 assert(!bts_audio_active && !bts_audio_burn_loop);
 assert(bts_audio_play(BTS_SOUND_BURN_OK,true));
 advance_to_end();
 assert(bts_audio_play(BTS_SOUND_UNDO,true));
 advance_to_end();
 assert(bts_audio_play(BTS_SOUND_LAMBDA_DISPLAY,true));
 advance_to_end();
 assert(bts_audio_play(BTS_SOUND_CYLINDER_OVER6,true));
 advance_to_end();
 assert(clear_count>14 && bts_audio_chunks_completed>100);
 bts_audio_close();
 assert(!bts_audio_ready);
 puts("PASS: 14 real PCM clips, all queued and drained, 16KB linear RAM, burn repeat/stop");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
 td=Path(td); (td/'test.c').write_text(native)
 (root/'build').mkdir(exist_ok=True)
 subprocess.run(['gcc','-c',str(root/'source/event_audio_assets.S'),'-o',str(td/'assets.o')],cwd=root/'build',check=True,capture_output=True)
 r=subprocess.run(['gcc','-std=gnu11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-function',
                   '-I'+str(root/'source'),str(td/'test.c'),str(td/'assets.o'),'-o',str(td/'run')],capture_output=True,text=True)
 assert r.returncode==0,r.stderr
 subprocess.run([str(td/'run')],check=True)
print('PASS: 3DS-style NDSP streaming mock and all cue data')
