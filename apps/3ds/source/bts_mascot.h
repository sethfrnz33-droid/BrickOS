#ifndef BTS_MASCOT_H
#define BTS_MASCOT_H
/* Brick Tuner Suite mascot: separate transparent upper head, lower jaw, and mouth interior.
 * Both the jaw movement and nonverbal PCM audio use ONE monotonic timeline.
 * This is presentation-only. It never sends ECU commands or changes calibrations. */
#include <stdint.h>
#include <string.h>
#include <math.h>
extern const uint8_t bts_mascot_atlas_start[],bts_mascot_atlas_end[];
extern const uint8_t bts_mascot_pcm_start[],bts_mascot_pcm_end[];
static C2D_SpriteSheet bts_mascot_sheet=NULL;
static s16 *bts_mascot_sound=NULL;
static ndspWaveBuf bts_mascot_wave;
static u32 bts_mascot_samples=0;
static u64 bts_mascot_voice_ms=0;
static u64 bts_mascot_notice_ms=0;

static void bts_mascot_init(void){
 const size_t tex_len=(size_t)(bts_mascot_atlas_end-bts_mascot_atlas_start);
 if(tex_len)bts_mascot_sheet=C2D_SpriteSheetLoadFromMem(bts_mascot_atlas_start,tex_len);
 const size_t bytes=(size_t)(bts_mascot_pcm_end-bts_mascot_pcm_start);
 if(!bts_ndsp_active||bytes<2||(bytes&1)||bytes>22050u*5u*2u)return;
 bts_mascot_sound=(s16*)linearAlloc(bytes);
 if(!bts_mascot_sound)return;
 memcpy(bts_mascot_sound,bts_mascot_pcm_start,bytes);
 DSP_FlushDataCache(bts_mascot_sound,bytes);
 bts_mascot_samples=(u32)(bytes/2);
 memset(&bts_mascot_wave,0,sizeof(bts_mascot_wave));
 bts_mascot_wave.data_pcm16=bts_mascot_sound;
 bts_mascot_wave.nsamples=bts_mascot_samples;
 bts_mascot_wave.looping=false;
}
static void bts_mascot_say(void){
 const u64 now=osGetTime();
 bts_mascot_notice_ms=now?now:1;
 if(!bts_ndsp_active||!bts_mascot_sound||!bts_mascot_samples||!event_audio_enabled)return;
 /* Dedicated channel 2: no interruption of critical event/burn audio on 0. */
 ndspChnWaveBufClear(2);
 memset(&bts_mascot_wave,0,sizeof(bts_mascot_wave));
 bts_mascot_wave.data_pcm16=bts_mascot_sound;
 bts_mascot_wave.nsamples=bts_mascot_samples;
 bts_mascot_wave.looping=false;
 ndspChnSetFormat(2,NDSP_FORMAT_MONO_PCM16);
 ndspChnSetRate(2,22050.0f);
 ndspChnSetInterp(2,NDSP_INTERP_LINEAR);
 float mix[12]={0};mix[0]=0.50f;mix[1]=0.50f;ndspChnSetMix(2,mix);
 ndspChnWaveBufAdd(2,&bts_mascot_wave);
 bts_mascot_voice_ms=now?now:1;
}
static float bts_mascot_jaw_gap(u64 now){
 if(!bts_mascot_voice_ms||now<bts_mascot_voice_ms)return 0.0f;
 u64 elapsed=now-bts_mascot_voice_ms;
 static const unsigned duration_ms[]={178,138,158,128,198,148,168};
 for(unsigned i=0;i<sizeof(duration_ms)/sizeof(duration_ms[0]);i++){
   unsigned d=duration_ms[i];
   if(elapsed<d){
     float t=(float)elapsed/(float)d;
     /* Follows actual synthesized syllable amplitude, closed at every gap. */
     float wave=sinf(3.14159265359f*t);
     return 6.0f*wave*wave;
   }
   elapsed-=d;
 }
 return 0.0f;
}
static void bts_mascot_draw(float x,float y,float scale){
 if(!bts_mascot_sheet)return;
 if(C2D_SpriteSheetCount(bts_mascot_sheet)<3)return;
 const float jaw=bts_mascot_jaw_gap(osGetTime())*scale;
 C2D_Image interior=C2D_SpriteSheetGetImage(bts_mascot_sheet,0);
 C2D_Image upper=C2D_SpriteSheetGetImage(bts_mascot_sheet,1);
 C2D_Image lower=C2D_SpriteSheetGetImage(bts_mascot_sheet,2);
 C2D_DrawImageAt(interior,x,y,0,NULL,scale,scale);
 C2D_DrawImageAt(upper,x,y,0,NULL,scale,scale);
 C2D_DrawImageAt(lower,x,y+jaw,0,NULL,scale,scale);
}
static bool bts_mascot_notice_active(u64 now){
 return bts_mascot_notice_ms&&now>=bts_mascot_notice_ms&&now-bts_mascot_notice_ms<3100ULL;
}
static void bts_mascot_shutdown(void){
 if(bts_ndsp_active)ndspChnWaveBufClear(2);
 if(bts_mascot_sound)linearFree(bts_mascot_sound);
 bts_mascot_sound=NULL;bts_mascot_samples=0;
 if(bts_mascot_sheet)C2D_SpriteSheetFree(bts_mascot_sheet);
 bts_mascot_sheet=NULL;
}
#endif
