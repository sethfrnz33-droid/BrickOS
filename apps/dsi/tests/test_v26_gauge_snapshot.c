#include "tuner.h"
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
int main(int argc,char**argv){
    assert(argc>1);
    assert(bt_sd_init());
    FILE* f=fopen(argv[1],"rb");assert(f);
    assert(bt_sd_begin());
    static BtDefinition original, reloaded;
    bt_ini_reset(&original);
    unsigned char bytes[2511];size_t n;
    while((n=fread(bytes,1,sizeof bytes,f))>0){
        assert(bt_sd_append(bytes,n));
        bt_ini_feed(&original,bytes,n);
    }
    fclose(f);
    assert(bt_sd_end()&&bt_sd_commit());
    assert(bt_ini_finish(&original));
    assert(original.gauge_count>350);
    assert(bt_sd_snapshot_save(&original));
    assert(bt_sd_snapshot_load(&reloaded));
    assert(reloaded.gauge_count==original.gauge_count);
    assert(reloaded.ref_count==original.ref_count);
    const BtGauge*g=bt_gauge(&reloaded,"pedalPositionGauge");
    assert(g&&g->channel[0]);
    int gauge_refs=0;
    for(int i=0;i<reloaded.ref_count;i++)if(reloaded.refs[i].kind==7)gauge_refs++;
    /* Additional [readoutPanel] gauge templates are now included. */
    assert(gauge_refs>=67);
    printf("PASS v26 SD snapshot restores %d gauge templates, %d gauge references, and placement metadata\n", reloaded.gauge_count,gauge_refs);
    bt_ini_reset(&original);bt_ini_reset(&reloaded);
    return 0;
}
