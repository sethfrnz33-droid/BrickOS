#include "tuner.h"
#include "wire.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
static BtDefinition d;
int main(int argc,char**argv){
    const char*ini="[TunerStudio]\nsignature = \"test-ecu-v1\"\n"
        "[Constants]\npageIdentifier = \"\\x00\\x01\"\npageSize = 256\n"
        "pageChunkWrite = C%2i%2o%2c%v\nburnCommand = B%2i\n"
        "page = 1\nve = array, U16, 32, [2], %, 0.1, 0, 0, 200\n"
        "[OutputChannels]\nRPMValue = scalar, U16, 0, RPM, 1, 0\n";
    bt_ini_reset(&d);bt_ini_feed(&d,(const uint8_t*)ini,strlen(ini));
    assert(bt_ini_finish(&d));
    const BtField*f=bt_field(&d,"ve",false);assert(f);
    uint8_t before[4]={100,0,200,0},after[4]={110,0,200,0};
    assert(bt_write_begin(&d,f,before,after,4,0));
    assert(bt_write_busy());
    if(argc>1&&!strcmp(argv[1],"cancel_prearm")){
        assert(bt_write_poll(1)==0);
        assert(bt_write_cancel());
        assert(!bt_write_busy());
        puts("WRITE_RESULT -1: CANCELLED BEFORE ARM");return 0;
    }
    if(argc>1&&!strcmp(argv[1],"wifi_lost_prearm")){
        bt_write_link_lost();
        assert(!bt_write_busy());
        puts("WRITE_RESULT -1: WIFI LOST BEFORE ARM");return 0;
    }
    bool attempted_postarm_cancel=false;
    /* bt_write_poll must not block on a missing response; the caller can
     * continue to draw frames and inspect progress during the transaction. */
    int done=0;
    const char *previous="";
    for(int frame=1;frame<=2000;frame++){
        done=bt_write_poll(frame);
        if(argc>1&&!strcmp(argv[1],"cancel_after_arm") &&
           !attempted_postarm_cancel && !strcmp(bt_write_phase(),"WRITING ECU CHANGE")){
            assert(!bt_write_cancel());attempted_postarm_cancel=true;
        }
        if(argc>1&&!strcmp(argv[1],"wifi_lost_after_arm") &&
           !attempted_postarm_cancel && !strcmp(bt_write_phase(),"WRITING ECU CHANGE")){
            bt_write_link_lost();attempted_postarm_cancel=true;
        }
        if(strcmp(previous,bt_write_phase())){fprintf(stderr,"frame %d: %s (network: %s)\n",frame,bt_write_phase(),bt_async_error());previous=bt_write_phase();}
        if(done)break;
        usleep(1000);
    }
    fprintf(stderr,"WRITE_RESULT %d: %s\n",done,bt_write_message());
    if(argc>1&&(!strcmp(argv[1],"success")||!strcmp(argv[1],"cancel_after_arm"))){
        assert(done==1);assert(!bt_write_busy());
        assert(!strcmp(bt_write_message(),"BURN VERIFIED / READBACK MATCHED"));
    }else{
        assert(done==-1);assert(!bt_write_busy());
    }
    if(argc>1&&(!strcmp(argv[1],"wifi_lost_after_arm")||
       !strcmp(argv[1],"cancel_after_arm")))assert(attempted_postarm_cancel);
    bt_async_cancel();bt_async_forget_connection();
    return 0;
}
