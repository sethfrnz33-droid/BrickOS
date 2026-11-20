/* No ECU required: verify user-initiated cache deletion and runtime reset.
   Compile against DSi UI and storage with unused sections discarded. */
#define main bts_dsi_entrypoint
#include "../source/main.c"
#undef main
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdlib.h>

void bt_async_cancel(void){}
void bt_async_forget_connection(void){}
void bt_async_expected_signature(const char *s){(void)s;}
void bt_cache_cancel(void){}
bool bt_write_busy(void){return false;}

static void write_file(const char *name,const char *txt){
    char path[256];snprintf(path,sizeof(path),"%s/%s",bt_sd_asset_root(),name);
    FILE *f=fopen(path,"wb");assert(f);
    assert(fwrite(txt,1,strlen(txt),f)==strlen(txt));
    assert(fclose(f)==0);
}
static bool exists(const char *name){
    char path[256];snprintf(path,sizeof(path),"%s/%s",bt_sd_asset_root(),name);
    struct stat st;return stat(path,&st)==0;
}
static void make_ini_definition(void){
    const char *ini="[TunerStudio]\nsignature=\"test.1234567890\"\n"
      "[Constants]\npage = 1\nx = scalar, U16, 0, \"\",1,0,0,100,0\n"
      "[OutputChannels]\nRPMValue = scalar,U16,0,\"RPM\",1,0\n";
    bt_ini_reset(&def);bt_ini_feed(&def,(const uint8_t*)ini,strlen(ini));
    assert(bt_ini_finish(&def));ini_ok=true;
}
int main(void){
    /* Run under BTS_HOST_CACHE_DIR set by the test runner. */
    assert(!bt_sd_delete_ini_cache());
    assert(bt_sd_init());sd_ready=true;
    make_ini_definition();
    strcpy(actual_sig,"test.1234567890");
    strcpy(favorite_signature,"test.1234567890");
    favorites_loaded=true;favorite_count=2;
    write_file("ecu.ini","old");write_file("ecu.meta","stale snapshot");
    write_file("ecu.meta.tmp","old tmp");write_file("ecu.bak","old backup");
    write_file("ecu-unverified.ini","unverified");write_file("ecu.tmp","partial");
    write_file("dsi-favorites.cfg","KEEP");write_file("clutch-source.cfg","KEEP");
    write_file("nierniernier.pcm8","KEEP");
    cache_loaded=true;loading_cache=true;loading_ini=true;
    frame=135;want_status=want_ini=false;
    replace_saved_ini();
    assert(!def.valid && !ini_ok && !cache_loaded && !loading_cache && !loading_ini);
    assert(def.channel_count==0 && def.constant_count==0 && def.map_count==0);
    assert(actual_sig[0]==0 && !favorites_loaded && favorite_count==0);
    assert(want_status&&want_ini && network_poll==135);
    const char *retained[]={"ecu.ini","ecu.meta","ecu.meta.tmp","ecu.bak",
       "ecu-unverified.ini","ecu.tmp"};
    for(unsigned i=0;i<sizeof retained/sizeof retained[0];i++)assert(exists(retained[i]));
    assert(exists("dsi-favorites.cfg"));assert(exists("clutch-source.cfg"));
    assert(exists("nierniernier.pcm8"));
    /* The old explicit deletion API is still available for isolated host
     * tests, but the refresh UI deliberately never uses it. */
    assert(bt_sd_delete_ini_cache());
    for(unsigned i=0;i<sizeof retained/sizeof retained[0];i++)assert(!exists(retained[i]));
    make_ini_definition();assert(ini_ok);
    puts("PASS v36: INI refresh preserves old cache; explicit deletion helper remains isolated; favorites/audio untouched");
    bt_ini_reset(&def);
    return 0;
}
