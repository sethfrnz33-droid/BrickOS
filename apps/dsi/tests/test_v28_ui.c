/* Host-only static UI regression: parse the user's complete ECU INI, then
 * verify the indexed dialog tree preserves original order and usable labels. */
#define main bts_dsi_entrypoint
#include "../source/main.c"
#undef main
#include <assert.h>
#include <time.h>
static int linear_owner_count(const char *owner){
    int n=0;for(int i=0;i<def.ref_count;i++)
        if(!strcasecmp(def.refs[i].owner,owner))n++;
    return n;
}
static int indexed_owner_count(const char *owner){
    int n=0,previous=-1;
    for(int i=owner_first(owner);i>=0;i=ref_chain[i]){
        if(strcasecmp(def.refs[i].owner,owner))continue;
        assert(i>previous);previous=i;n++;
    }
    return n;
}
int main(int argc,char **argv){
    assert(argc==2);
    FILE*f=fopen(argv[1],"rb");assert(f);
    bt_ini_reset(&def);
    unsigned char buf[1907];size_t n;
    while((n=fread(buf,1,sizeof(buf),f))>0)bt_ini_feed(&def,buf,n);
    fclose(f);assert(bt_ini_finish(&def));
    assert(!def.truncated&&!def.out_of_memory);
    reset_ref_index();
    unsigned before=ref_index_builds;
    const char *owners[]={"injectionSettings","injectionSettingsWest",
        "injectionSettingsEast","idleSettings","nonexistentDialog","triggerSettings"};
    for(size_t i=0;i<sizeof(owners)/sizeof(owners[0]);i++){
        assert(indexed_owner_count(owners[i])==linear_owner_count(owners[i]));
    }
    assert(ref_index_builds==before+1);
    for(int i=0;i<def.ref_count;i+=37)
        assert(indexed_owner_count(def.refs[i].owner)==linear_owner_count(def.refs[i].owner));
    assert(ref_index_builds==before+1);
    strcpy(dialog_name,"injectionSettings");
    rebuild_dialog_entries();
    int count=dialog_count();assert(count>=1&&count<=512);
    for(int i=0;i<count;i++)assert(dialog_at(i)>=0&&dialog_at(i)<def.ref_count);
    /* A different real dialog from this INI contains a gauge template. */
    int gauge_index=-1;
    for(int i=0;i<def.ref_count;i++)if(def.refs[i].kind==7){gauge_index=i;break;}
    assert(gauge_index>=0);
    snprintf(dialog_name,sizeof(dialog_name),"%s",def.refs[gauge_index].owner);
    rebuild_dialog_entries();
    rebuild_dialog_gauges();
    assert(dialog_gauge_total>=1);
    for(int i=0;i<dialog_gauge_total;i++){
        int index=dialog_gauge_at(i);
        assert(index>=0&&index<def.ref_count&&def.refs[index].kind==7);
    }
    /* 9x14 folder labels must use the readable normal glyph raster, not
     * 6x9 tiny text; no drawing outside either 256x192 framebuffer. */
    memset(draw_bottom,0,sizeof(draw_bottom));
    menu_text(draw_bottom,9,32,"Fuel Injection Settings",WHITE,204);
    int bottommost=-1;
    for(int y=32;y<50;y++)for(int x=9;x<230;x++)if(draw_bottom[y*W+x])bottommost=y;
    assert(bottommost>=42&&bottommost<=45);
    memset(draw_bottom,0,sizeof(draw_bottom));
    menu_text(draw_bottom,200,32,"Long name clipped",WHITE,52);
    for(int y=0;y<H;y++)for(int x=252;x<W;x++)assert(draw_bottom[y*W+x]==0);
    printf("PASS DSi v28 UI: %d INI refs; %d nested entries; %d reference gauges; 9x14 folder text; single index build\n",
           def.ref_count,count,dialog_gauge_total);
    bt_ini_reset(&def);
    return 0;
}
