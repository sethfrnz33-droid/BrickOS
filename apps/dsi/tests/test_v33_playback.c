/* Host test that every converted clutch PCM file can be opened for playback. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "sfx.h"
static const char *root;
const char *bt_sd_asset_root(void){return root;}
int main(int argc,char **argv){
    assert(argc==2);root=argv[1];
    for(int i=0;i<7;i++){
        assert(bts_sfx_play_clutch(i,true));
    }
    assert(!bts_sfx_play_clutch(-1,true));
    assert(!bts_sfx_play_clutch(7,true));
    assert(!bts_sfx_play_clutch(0,false));
    assert(bts_sfx_play_clutch(0,true));
    bts_sfx_shutdown();
    puts("PASS v33: 7 converted PCM cues open and play through the DSi API stub");
    return 0;
}
