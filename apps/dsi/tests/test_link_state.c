/* Actual DSi main-loop state transitions, compiled against native host stubs. */
#define main brick_dsi_boot_stub
#include "../source/main.c"
#undef main
bool bts_sfx_play(int cue,bool sd_ready){(void)cue;(void)sd_ready;return true;}
void bts_sfx_shutdown(void){}
bool bts_sfx_files_ready(bool sd){(void)sd;return true;}
const char *bts_sfx_status(bool sd){(void)sd;return "AUDIO OK";}
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void){
    bt_ini_reset(&def);
    wifi_up=true;
    frame=100;
    check_status("{\"ecu_session_connected\":true,"
                 "\"ecu_signature\":\"brick.1293678056\"}");
    assert(ecu_up && ecu_status_known && bridge_up);
    assert(!strcmp(actual_sig,"brick.1293678056"));
    check_status("{\"ecu_session_connected\":true,"
                 "\"ecu_signature\":\"anotherName.1293678056\"}");
    assert(ecu_up && ecu_status_known && bridge_up);
    assert(!strcmp(actual_sig,"anotherName.1293678056"));
    /* Failed HTTP status must not invent an ECU disconnection. */
    network_finished(BT_NET_STATUS,-1);
    assert(ecu_status_known && ecu_up && bridge_up);
    assert(network_resume_frame>=frame+120);
    assert(want_status);
    int backoff=network_resume_frame;
    frame=backoff-1;
    int last=last_socket_begin_frame;
    service_network();
    assert(last_socket_begin_frame==last);
    /* Recover when BrickOS actually returns a valid status response. */
    network_success(BT_NET_STATUS);
    check_status("{\"ecu_session_connected\":true,"
                 "\"ecu_signature\":\"anotherName.1293678056\"}");
    assert(ecu_status_known && ecu_up);
    /* Only an explicit status response is allowed to say ECU disconnected. */
    check_status("{\"ecu_session_connected\":false}");
    assert(ecu_status_known && !ecu_up && bridge_up);
    /* Regression: a valid live frame followed by failed live HTTP must NOT
     * mark a previously confirmed ECU session disconnected or queue status
     * polls on every gauge-frame error. */
    check_status("{\"ecu_session_connected\":true,"
                 "\"ecu_signature\":\"anotherName.1293678056\"}");
    want_status=false;
    live_failure_streak=0;
    live_resume_frame=0;
    network_resume_frame=0;
    frame=5000;
    network_finished(BT_NET_LIVE,-1);
    assert(ecu_up && ecu_status_known && bridge_up);
    assert(!want_status);
    assert(live_failure_streak==1);
    assert(live_resume_frame==5120);
    assert(network_resume_frame==0);
    frame=5001;
    network_finished(BT_NET_LIVE,-1);
    assert(live_failure_streak==2);
    assert(live_resume_frame==5241);
    network_success(BT_NET_LIVE);
    assert(live_failure_streak==0 && live_resume_frame==0);
    /* Status errors retain the last-confirmed session, not WAITING flicker. */
    network_finished(BT_NET_STATUS,-1);
    assert(ecu_up && ecu_status_known);
    assert(bridge_up);
    network_success(BT_NET_STATUS);
    check_status("{\"ecu_session_connected\":true,"
                 "\"ecu_signature\":\"anotherName.1293678056\"}");
    /* Exponential backoff cannot grow without limit. */
    for(int i=0;i<20;i++){network_failure();assert(network_resume_frame-frame<=BTS_MAX_BACKOFF_FRAMES);}
    puts("PASS: names do not reset sessions; HTTP failure is unknown; E12 backoff; explicit disconnect");
    return 0;
}
