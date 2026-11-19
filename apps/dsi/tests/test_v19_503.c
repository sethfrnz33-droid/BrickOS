/* Regression: ESP returns HTTP 503 while its live worker warms a sample.
 * This must not extend the retry gap beyond the ESP's four-second interest TTL. */
static int mock_http_status=503;
#define bt_async_http_status test_http_status
#define main brick_dsi_boot_stub
#include "../source/main.c"
#undef main
#undef bt_async_http_status
int test_http_status(void){return mock_http_status;}
bool bts_sfx_play(int cue,bool sd){(void)cue;(void)sd;return true;}
void bts_sfx_shutdown(void){}
bool bts_sfx_files_ready(bool sd){(void)sd;return true;}
const char *bts_sfx_status(bool sd){(void)sd;return "AUDIO OK";}
#include <assert.h>
#include <stdio.h>
int main(void){
    wifi_up=bridge_up=ecu_up=true;
    ecu_status_known=true;
    frame=1000;
    for(int i=0;i<6;i++){
        frame+=24;
        network_finished(BT_NET_LIVE,-1);
        assert(live_resume_frame==frame+6);
        assert(live_failure_streak==0);
        assert(ecu_up&&ecu_status_known&&bridge_up&&!want_status);
    }
    mock_http_status=0;
    frame+=24;
    network_finished(BT_NET_LIVE,-1);
    assert(live_resume_frame==frame+120);
    assert(live_failure_streak==1);
    puts("PASS: 503 live worker warmup keeps subscription alive, socket errors back off");
}
