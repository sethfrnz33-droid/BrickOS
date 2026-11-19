#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sfx.h"
#include "storage.h"
#include "bts_audio_rules.h"
/* DSi: PCM8 @ 11025Hz files on SD, converted from the v62 PCM16 assets.
 * No audio is loaded during boot or network requests. Replaces a previous cue
 * only on a navigation event, not while hovering or rendering a frame. */
static signed char *active_pcm;
static int active_channel=-1;
static char audio_status[96]="AUDIO NOT TESTED";
static int audio_assets_state=-1;
static void sfx_message(const char *value){snprintf(audio_status,sizeof(audio_status),"%s",value);}
bool bts_sfx_files_ready(bool sd_ready){
    if(!sd_ready)return false;
    if(audio_assets_state>=0)return audio_assets_state==1;
    /* Do not touch SD for the 30Hz draw loop after this first check. */
    char path[160];snprintf(path,sizeof(path),"%s/sfx/00.pcm8",bt_sd_asset_root());
    FILE *f=fopen(path,"rb");
    audio_assets_state=f?1:0;
    if(f)fclose(f);
    return audio_assets_state==1;
}
const char *bts_sfx_status(bool sd_ready){
    if(!sd_ready)return "AUDIO: SD NOT MOUNTED";
    if(!bts_sfx_files_ready(sd_ready))return "AUDIO: MISSING /BrickECU/sfx/";
    return audio_status;
}

static void stop_active(void){
    if(active_channel>=0)soundKill(active_channel);
    active_channel=-1;
    free(active_pcm);active_pcm=NULL;
}
bool bts_sfx_play(int cue,bool sd_ready){
    if(!sd_ready){sfx_message("AUDIO: SD NOT MOUNTED");return false;}
    if(cue<0||cue>=BTS_SOUND_COUNT){sfx_message("AUDIO: INVALID SOUND CUE");return false;}
    char filename[160];snprintf(filename,sizeof(filename),"%s/sfx/%02d.pcm8",bt_sd_asset_root(),cue);
    FILE *f=fopen(filename,"rb");if(!f){sfx_message("MISSING PCM8 ON SD /BrickECU/sfx/");audio_assets_state=0;return false;}
    if(fseek(f,0,SEEK_END)!=0){fclose(f);sfx_message("AUDIO FILE SEEK ERROR");return false;}
    long len=ftell(f);
    if(len<32||len>500000||fseek(f,0,SEEK_SET)!=0){fclose(f);sfx_message("AUDIO LENGTH/FORMAT ERROR");return false;}
    signed char *pcm=(signed char*)malloc((size_t)len);
    if(!pcm){fclose(f);sfx_message("NO MEMORY FOR SOUND");return false;}
    size_t n=fread(pcm,1,(size_t)len,f);
    fclose(f);
    if(n!=(size_t)len){free(pcm);sfx_message("AUDIO SD READ ERROR");return false;}
    stop_active();
    /* ARM9 fread fills cached main RAM. The sound engine reads PCM directly
     * from physical memory, so flush the newly loaded sample before starting
     * the ARM7-visible playback buffer. No per-frame cache flush is needed. */
    DC_FlushAll();
    active_channel=soundPlaySample(pcm,SoundFormat_8Bit,(u32)n,11025,105,64,false,0);
    if(active_channel<0){free(pcm);sfx_message("DS SOUND CHANNEL ERROR");return false;}
    active_pcm=pcm;
    sfx_message("AUDIO: PLAYBACK REQUESTED");
    return true;
}
void bts_sfx_shutdown(void){stop_active();}
