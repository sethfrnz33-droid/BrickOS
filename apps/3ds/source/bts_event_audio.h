#ifndef BTS_EVENT_AUDIO_H
#define BTS_EVENT_AUDIO_H
/* Brick Tuner Suite: embedded mono PCM16 (22.05 kHz), streamed to the 3DS DSP.
 * Two 4096-frame linear-memory buffers (<17 KiB total) replace the old
 * 470 KiB one-shot buffer. No SD files or decoder are required.
 * Call bts_audio_pump() every UI frame. Only the main/UI thread owns NDSP.
 */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "bts_audio_rules.h"
#define BTS_AUDIO_MAX_SAMPLES (22050u * 36u)
#define BTS_AUDIO_BLOCK_SAMPLES 4096u
#define BTS_AUDIO_BUFFER_COUNT 2
extern const uint8_t bts_embedded_0_start[], bts_embedded_0_end[];
extern const uint8_t bts_embedded_1_start[], bts_embedded_1_end[];
extern const uint8_t bts_embedded_2_start[], bts_embedded_2_end[];
extern const uint8_t bts_embedded_3_start[], bts_embedded_3_end[];
extern const uint8_t bts_embedded_4_start[], bts_embedded_4_end[];
extern const uint8_t bts_embedded_5_start[], bts_embedded_5_end[];
extern const uint8_t bts_embedded_6_start[], bts_embedded_6_end[];
extern const uint8_t bts_embedded_7_start[], bts_embedded_7_end[];
extern const uint8_t bts_embedded_8_start[], bts_embedded_8_end[];
extern const uint8_t bts_embedded_9_start[], bts_embedded_9_end[];
extern const uint8_t bts_embedded_10_start[], bts_embedded_10_end[];
extern const uint8_t bts_embedded_11_start[], bts_embedded_11_end[];
extern const uint8_t bts_embedded_12_start[], bts_embedded_12_end[];
extern const uint8_t bts_embedded_13_start[], bts_embedded_13_end[];

typedef struct {const uint8_t *data;u32 count;} BtsAudioClip;
static BtsAudioClip bts_audio_clips[BTS_SOUND_COUNT];
static s16 *bts_audio_shared=NULL; /* 2 x 4096 samples, DSP accessible */
static size_t bts_audio_capacity=0;
static ndspWaveBuf bts_audio_wave[BTS_AUDIO_BUFFER_COUNT];
static bool bts_audio_ready=false;
static bool bts_audio_active=false;
static bool bts_audio_burn_loop=false;
static unsigned bts_audio_loaded=0;
static unsigned bts_audio_play_attempts=0;
static unsigned bts_audio_play_queued=0;
static unsigned bts_audio_chunks_queued=0;
static unsigned bts_audio_chunks_completed=0;
static int bts_audio_last_cue=-1;
static bool bts_audio_last_ok=false;
static int bts_audio_now_playing=-1;
static u32 bts_audio_next_sample=0;
static unsigned bts_audio_bad_clip_count=0;
static const char *bts_audio_error="NONE";

static bool bts_load_embedded(BtsAudioClip *c,const uint8_t *start,const uint8_t *end){
 if(!c||!start||!end||end<=start)return false;
 size_t sample_bytes=(size_t)(end-start);
 if(sample_bytes==0||(sample_bytes&1)||sample_bytes>2u*BTS_AUDIO_MAX_SAMPLES)return false;
 c->data=start;c->count=(u32)(sample_bytes/2);return true;
}
static void bts_audio_init(bool ndsp_initialized){
 if(!ndsp_initialized){bts_audio_error="DSP OFF";return;}
 const uint8_t *const beginnings[BTS_SOUND_COUNT]={
  bts_embedded_0_start,bts_embedded_1_start,bts_embedded_2_start,bts_embedded_3_start,
  bts_embedded_4_start,bts_embedded_5_start,bts_embedded_6_start,bts_embedded_7_start,
  bts_embedded_8_start,bts_embedded_9_start,bts_embedded_10_start,
  bts_embedded_11_start,bts_embedded_12_start,bts_embedded_13_start};
 const uint8_t *const ends[BTS_SOUND_COUNT]={
  bts_embedded_0_end,bts_embedded_1_end,bts_embedded_2_end,bts_embedded_3_end,
  bts_embedded_4_end,bts_embedded_5_end,bts_embedded_6_end,bts_embedded_7_end,
  bts_embedded_8_end,bts_embedded_9_end,bts_embedded_10_end,
  bts_embedded_11_end,bts_embedded_12_end,bts_embedded_13_end};
 for(int i=0;i<BTS_SOUND_COUNT;i++){
  if(bts_load_embedded(&bts_audio_clips[i],beginnings[i],ends[i]))bts_audio_loaded++;
  else bts_audio_bad_clip_count++;
 }
 if(bts_audio_loaded!=BTS_SOUND_COUNT){bts_audio_error="ASSET MISSING";return;}
 bts_audio_capacity=BTS_AUDIO_BUFFER_COUNT*BTS_AUDIO_BLOCK_SAMPLES*sizeof(s16);
 bts_audio_shared=(s16*)linearAlloc(bts_audio_capacity);
 if(!bts_audio_shared){bts_audio_error="LINEAR RAM";return;}
 memset(bts_audio_shared,0,bts_audio_capacity);
 memset(bts_audio_wave,0,sizeof(bts_audio_wave));
 bts_audio_ready=true;bts_audio_error="NONE";
}
static void bts_audio_stop(void){
 if(!bts_audio_ready)return;
 ndspChnWaveBufClear(0);
 bts_audio_active=false;bts_audio_burn_loop=false;
 bts_audio_now_playing=-1;bts_audio_next_sample=0;
 for(int i=0;i<BTS_AUDIO_BUFFER_COUNT;i++){
  bts_audio_wave[i].looping=false;
  bts_audio_wave[i].status=NDSP_WBUF_FREE;
 }
}
static void bts_audio_stop_burn_loop(void){
 if(bts_audio_burn_loop)bts_audio_stop();
}
/* Queue one block. The mixer must not touch a queued/playing data buffer. */
static bool bts_audio_queue_block(int slot){
 if(!bts_audio_active||bts_audio_now_playing<0||slot<0||slot>=BTS_AUDIO_BUFFER_COUNT)return false;
 const BtsAudioClip *c=&bts_audio_clips[bts_audio_now_playing];
 if(!c->data||!c->count)return false;
 if(bts_audio_next_sample>=c->count){
  if(!bts_audio_burn_loop)return false;
  bts_audio_next_sample=0;
 }
 u32 frames=c->count-bts_audio_next_sample;
 if(frames>BTS_AUDIO_BLOCK_SAMPLES)frames=BTS_AUDIO_BLOCK_SAMPLES;
 s16 *destination=bts_audio_shared+slot*BTS_AUDIO_BLOCK_SAMPLES;
 memcpy(destination,c->data+(size_t)bts_audio_next_sample*sizeof(s16),(size_t)frames*sizeof(s16));
 if(frames<BTS_AUDIO_BLOCK_SAMPLES)
    memset(destination+frames,0,(BTS_AUDIO_BLOCK_SAMPLES-frames)*sizeof(s16));
 DSP_FlushDataCache(destination,BTS_AUDIO_BLOCK_SAMPLES*sizeof(s16));
 memset(&bts_audio_wave[slot],0,sizeof(bts_audio_wave[slot]));
 bts_audio_wave[slot].data_pcm16=destination;
 bts_audio_wave[slot].nsamples=frames;
 bts_audio_wave[slot].looping=false; /* looping is handled at block boundaries */
 ndspChnWaveBufAdd(0,&bts_audio_wave[slot]);
 bts_audio_chunks_queued++;
 bts_audio_next_sample+=frames;
 return true;
}
static bool bts_audio_play_mode(int cue,bool enabled,bool loop){
 /* Most INI entries have no associated sound. An absent cue is a no-op,
  * not a playback failure and must not overwrite the last real event. */
 if(cue<0||cue>=BTS_SOUND_COUNT)return false;
 bts_audio_last_cue=cue;bts_audio_last_ok=false;bts_audio_play_attempts++;
 if(!enabled){bts_audio_error="AUTO OFF";return false;}
 if(!bts_audio_ready){bts_audio_error="PLAYER NOT READY";return false;}
 if(bts_audio_burn_loop){bts_audio_error="BURN AUDIO BUSY";return false;}
 const BtsAudioClip *c=&bts_audio_clips[cue];
 if(!c->data||!c->count){bts_audio_error="CLIP EMPTY";return false;}
 bts_audio_stop(); /* interrupts menu beep or the previous event */
 bts_audio_burn_loop=loop;bts_audio_active=true;
 bts_audio_now_playing=cue;bts_audio_next_sample=0;
 ndspChnSetFormat(0,NDSP_FORMAT_MONO_PCM16);
 ndspChnSetRate(0,22050.0f);
 ndspChnSetInterp(0,NDSP_INTERP_LINEAR);
 float mix[12]={0};mix[0]=1.0f;mix[1]=1.0f;ndspChnSetMix(0,mix);
 bool first=bts_audio_queue_block(0);
 bool second=bts_audio_queue_block(1);
 (void)second;
 if(!first){bts_audio_stop();bts_audio_error="DSP QUEUE FAILED";return false;}
 bts_audio_last_ok=true;bts_audio_error="NONE";bts_audio_play_queued++;
 return true;
}
static bool bts_audio_play(int cue,bool enabled){return bts_audio_play_mode(cue,enabled,false);}
static bool bts_audio_start_burn_loop(bool enabled){return bts_audio_play_mode(BTS_SOUND_BURN_START,enabled,true);}
/* Called every main loop iteration; NDSP_WBUF_DONE is the only safe state
 * to reuse a queued sample buffer. This follows devkitPro's streaming example. */
static void bts_audio_pump(void){
 if(!bts_audio_active)return;
 bool pending=false;
 for(int i=0;i<BTS_AUDIO_BUFFER_COUNT;i++){
  u8 state=bts_audio_wave[i].status;
  if(state==NDSP_WBUF_DONE){
     bts_audio_chunks_completed++;
     if(bts_audio_queue_block(i))pending=true;
     else bts_audio_wave[i].status=NDSP_WBUF_FREE;
  }else if(state==NDSP_WBUF_PLAYING||state==NDSP_WBUF_QUEUED){pending=true;}
 }
 if(!pending){
   bts_audio_active=false;bts_audio_now_playing=-1;
   bts_audio_burn_loop=false;
 }
}
static void bts_audio_close(void){
 if(bts_audio_ready)bts_audio_stop();
 if(bts_audio_shared)linearFree(bts_audio_shared);
 bts_audio_shared=NULL;bts_audio_capacity=0;
 memset(bts_audio_wave,0,sizeof(bts_audio_wave));
 memset(bts_audio_clips,0,sizeof(bts_audio_clips));
 bts_audio_ready=false;bts_audio_loaded=0;bts_audio_error="CLOSED";
}
#endif
