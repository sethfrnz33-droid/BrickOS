/* Regression: explicit refresh must not delete the only good SD INI; an
 * empty optional ECU signature must not block read-only INI retrieval. */
#define main bts_dsi_entrypoint
#include "../source/main.c"
#undef main
#include <assert.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

bool bts_sfx_play(int cue,bool ready){(void)cue;(void)ready;return true;}
bool bts_sfx_files_ready(bool ready){return ready;}
const char *bts_sfx_status(bool ready){(void)ready;return "OK";}
void bts_sfx_shutdown(void){}

static void write_file(const char *name, const char *content){
    char path[200];snprintf(path,sizeof(path),"%s/%s",bt_sd_asset_root(),name);
    FILE *f=fopen(path,"wb");assert(f);
    assert(fwrite(content,1,strlen(content),f)==strlen(content));
    assert(fclose(f)==0);
}
static bool file_equals(const char *name, const char *expected){
    char path[200],body[512];snprintf(path,sizeof(path),"%s/%s",bt_sd_asset_root(),name);
    FILE *f=fopen(path,"rb");if(!f)return false;
    size_t n=fread(body,1,sizeof(body)-1,f);body[n]=0;fclose(f);
    return strcmp(body,expected)==0;
}
static void seed(const char *text){
    bt_ini_reset(&def);bt_ini_feed(&def,(const uint8_t*)text,strlen(text));
    assert(bt_ini_finish(&def));set_compat();
}
int main(int argc,char **argv){
    assert(argc==2);
    assert(bt_sd_init());sd_ready=true;
    const char *old="[TunerStudio]\nsignature=\"test.1234567890\"\n[Constants]\npage=1\nx=scalar,U16,0,\"\",1,0,0,100,0\n[OutputChannels]\nRPMValue=scalar,U16,0,\"RPM\",1,0\n";
    bool cold=!strcmp(argv[1],"missing")||!strcmp(argv[1],"offline");
    if(!cold){
        write_file("ecu.ini",old);
        seed(old);
        strcpy(actual_sig,"test.1234567890");
    }
    write_file("dsi-favorites.cfg","FAVS");
    wifi_up=true;bridge_up=ecu_up=!cold;
    frame=100;
    if(!cold){
        replace_saved_ini();
        assert(file_equals("ecu.ini",old));
        assert(!ini_ok && !def.valid && want_ini && want_status);
    }else{
        want_status=true;want_ini=false;
        assert(!file_equals("ecu.ini",old));
        assert(!def.valid);
    }
    assert(file_equals("dsi-favorites.cfg","FAVS"));
    bool succeeded=false,restored=false;
    for(frame=101;frame<2600;frame++){
        service_network();
        usleep(2000);
        if(def.valid && ini_ok && !loading_ini && !loading_cache){
            if(strcmp(argv[1],"failure")){
                if(bt_field(&def,"freshField",false)){
                    succeeded=true;break;
                }
            }else if(bt_field(&def,"x",false)){
                restored=true;break;
            }
        }
    }
    fprintf(stderr,"FINAL frame=%d mode=%s valid=%d ini=%d status=%s error=%s path=%s sig=%s ready=%d bridge=%d want=%d statusW=%d active=%d phase=%s pagefields=%d\n",frame,argv[1],def.valid,ini_ok,status_msg,last_net_error,last_failed_path,actual_sig,sd_ready,bridge_up,want_ini,want_status,bt_async_busy(),bt_async_phase(),def.constant_count);
    if(strcmp(argv[1],"failure")){
        assert(succeeded);
        assert(bt_field(&def,"freshField",false));
        assert(!file_equals("ecu.ini",old));
        printf("PASS: INI replacement downloaded without optional signature and validated\n");
    }else{
        assert(restored);
        assert(file_equals("ecu.ini",old));
        printf("PASS: failed refresh restored validated old INI without deleting it\n");
    }
    assert(file_equals("dsi-favorites.cfg","FAVS"));
    bt_async_cancel();bt_async_forget_connection();bt_ini_reset(&def);
    return 0;
}
