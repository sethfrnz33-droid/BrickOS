/* Cooperative HTTP regression suite. Tests real local sockets, not mock bytes. */
#include "wire.h"
#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <stdarg.h>

/* Build test with -DARM9 to exercise the DSWifi socket-close branch.
 * The POSIX shim counts close calls and proves sockets are released. */
static int socket_close_count=0,nonblock_ioctl_count=0,socket_alloc_count=0;
static int force_enomem_socket=0;
int socket(int domain,int type,int protocol){
    if(force_enomem_socket){force_enomem_socket--;errno=ENOMEM;return -1;}
    socket_alloc_count++;return (int)syscall(SYS_socket,domain,type,protocol);
}
int closesocket(int fd){socket_close_count++;return close(fd);}
/* The real DS stack owns its socket handles. On Linux, forward FIONBIO to
 * the kernel while counting it so tests cannot accidentally pass using
 * fcntl(F_GETFL) or a blocking-socket fallback. */
int ioctl(int fd,unsigned long request,...){
    va_list args;va_start(args,request);
    void *data=va_arg(args,void *);va_end(args);
    if(request==FIONBIO)nonblock_ioctl_count++;
    return (int)syscall(SYS_ioctl,fd,request,data);
}
static BtDefinition definition;
static BtLive live;
static int frame=0;
static void check(int ok,const char *message){
    if(!ok){fprintf(stderr,"FAIL: %s / phase=%s / err=%s / recv=%lu / HTTP=%d\n",
       message,bt_async_phase(),bt_async_error(),(unsigned long)bt_async_bytes(),bt_async_http_status());exit(1);}
}
static void run_to_finish(const char *message,int max_ticks){
    int result=0;
    for(int i=0;i<max_ticks&&result==0;i++){
        result=bt_async_poll(++frame);
        usleep(250);
    }
    check(result==1,message);
}
int main(void){
    check(bt_sd_init(),"initialize SD cache");
    bt_async_expected_signature("rusEFI master.2026.11.18.bigbrick.1293678056");
    check(bt_async_begin(BT_NET_STATUS,NULL,NULL,0,frame),"start status");
    run_to_finish("async status successful",400);
    check(strstr(bt_async_status_json(),"ecu_session_connected")!=NULL,"status payload");
    check(bt_async_bytes()>20,"status length");
    bt_async_cancel();

    check(bt_async_begin(BT_NET_INI,&definition,NULL,0,frame),"start streamed ini");
    int spins=0,progress=0,result=0,saw_draining=0,saw_parsing=0;
    size_t last_saved=0;
    for(;spins<12000&&result==0;spins++){
        result=bt_async_poll(++frame);
        if(bt_async_bytes()>94248)progress=1;
        size_t saved=bt_async_saved_bytes();
        check(saved>=last_saved,"SD saved bytes never decrease");
        check(saved<=bt_async_bytes(),"never save more than network received");
        last_saved=saved;
        if(strstr(bt_async_phase(),"FINISHING SD"))saw_draining=1;
        if(strstr(bt_async_phase(),"PARSING INI"))saw_parsing=1;
        usleep(300);
    }
    check(result==1,"async INI finished");
    check(progress,"INI transfer advanced beyond old 94248-byte stall");
    check(saw_draining&&saw_parsing,"receive, SD drain, parse are separate phases");
    check(bt_async_saved_bytes()>900000,"large INI saved to SD completely");
    check(bt_async_parsed_bytes()==bt_async_saved_bytes(),"whole SD copy parsed");
    check(definition.valid,"INI parser valid");
    {char path[512];snprintf(path,sizeof(path),"%s/signature-debug.txt","/tmp/BrickECU-DSi-SD-Test");
     FILE *f=fopen(path,"rb");check(f!=NULL,"signature diagnostic saved");
     char txt[1024];size_t n=fread(txt,1,sizeof(txt)-1,f);fclose(f);txt[n]=0;
     check(strstr(txt,"Layout text match (diagnostic only): YES")!=NULL,"layout comparison recorded for diagnosis");
     check(strstr(txt,"ESP32 HTTP response: 200 (OK)")!=NULL,"HTTP 200 not labelled error");}
    FILE *disk=bt_sd_open_cached();
    check(disk!=NULL,"INI was committed to SD cache");
    char head[24]={0};fread(head,1,13,disk);fclose(disk);
    check(!strncmp(head,"[TunerStudio]",13),"cached raw INI header");
    check(definition.channel_count>0,"INI channels parsed");
    check(definition.page_count==64,"64 page identifiers after 90KB parsed");
    bt_async_cancel();
    /* Persist a pointer-free, checked parsed metadata snapshot for instant
     * subsequent launches. Malformed/stale snapshots must never be trusted. */
    check(bt_sd_snapshot_save(&definition),"write parsed INI snapshot");
    int orig_channels=definition.channel_count,orig_maps=definition.map_count;
    bt_ini_reset(&definition);
    check(bt_sd_snapshot_load(&definition),"restore parsed metadata without parsing the text INI");
    check(definition.valid && definition.channel_count==orig_channels &&
          definition.map_count==orig_maps && definition.page_count==64,
          "snapshot restores decoded definitions");
    {const char *cached_path=bt_sd_cache_path();FILE*f=fopen(cached_path,"r+b");
     check(f!=NULL,"open raw cached INI for staleness test");
     check(fseek(f,1,SEEK_SET)==0,"seek for raw cache mutation");
     check(fputc('X',f)!=EOF,"mutate raw INI");check(fclose(f)==0,"close changed INI");
     check(!bt_sd_snapshot_load(&definition),"reject snapshot if raw INI changed");
     f=fopen(cached_path,"r+b");check(f!=NULL,"restore raw cache");
     check(fseek(f,1,SEEK_SET)==0,"seek to restore raw cache");
     check(fputc('T',f)!=EOF,"restore raw INI");check(fclose(f)==0,"close restored INI");
    }
    bt_ini_reset(&definition);
    check(bt_sd_snapshot_load(&definition),"restored raw file passes checksum");
    /* Replay the exact cached file without network, one SD chunk per frame. */
    bt_ini_reset(&definition);
    check(bt_cache_begin(&definition),"begin cached INI load");
    int cache_result=0;
    for(int i=0;i<2000 && cache_result==0;i++)cache_result=bt_cache_poll();
    check(cache_result==1 && definition.valid,"cached INI re-parsed");
    check(!strcmp(definition.signature,"BrickECU altboard.2025.04.12.generic.1293678056"),"cached ECU signature");
    check(bt_cache_bytes()>32768,"cached INI replay covered entire file");
    /* Reproduce the exact case the user reported: v62 accepts an INI even
     * when ECU status has different signature text (or hash), provided the
     * HTTP body and INI structure are complete. Never block loading on it. */
    bt_async_expected_signature("rusEFI master.2026.11.18.bigbrick.1293678057");
    check(bt_async_begin(BT_NET_INI,&definition,NULL,0,frame),"start differently named ECU INI download");
    int sig_result=0;
    for(int i=0;i<12000 && sig_result==0;i++){sig_result=bt_async_poll(++frame);usleep(300);}
    check(sig_result==1 && definition.valid,"v62-style INI acceptance independent of ECU text");
    check(!bt_async_signature_mismatch(),"INI download is not a signature validation error");
    {FILE *f=fopen("/tmp/BrickECU-DSi-SD-Test/signature-debug.txt","rb");
     check(f!=NULL,"different signature recorded for diagnosis");
     char txt[1024];size_t n=fread(txt,1,sizeof(txt)-1,f);fclose(f);txt[n]=0;
     check(strstr(txt,"Layout text match (diagnostic only): NO")!=NULL,"difference recorded truthfully");}
    bt_async_cancel();
    disk=bt_sd_open_cached();
    check(disk!=NULL,"parsed INI cached despite naming differences");
    fclose(disk);
    bt_async_expected_signature("rusEFI master.2026.11.18.bigbrick.1293678056");
    /* A DSWiFi E12 allocation failure must not delete the parsed menu/INI. */
    int previous_channels=definition.channel_count;
    force_enomem_socket=1;
    check(!bt_async_begin(BT_NET_INI,&definition,NULL,0,frame),"simulated E12 aborts connection");
    check(strstr(bt_async_error(),"SOCKET FAIL E12 H0")!=NULL,"E12 reported with no fake HTTP code");
    check(definition.valid && definition.channel_count==previous_channels,
          "failed new socket keeps previously parsed ECU INI");
    bt_async_cancel();

    check(bt_async_begin(BT_NET_LIVE,NULL,&live,256,frame),"start live");
    run_to_finish("live frame successful",400);
    check(live.valid,"live parsed correctly");
    bt_async_cancel();
    /* The real ESP failure is a non-200 HTTP code; preserve its endpoint,
     * literal status line and short error body for hardware diagnosis. */
    check(bt_async_begin(BT_NET_LIVE,NULL,&live,300,frame),"start intentionally rejected HTTP request");
    int rejected=0;
    for(int i=0;i<600&&rejected==0;i++){
        rejected=bt_async_poll(++frame);usleep(250);
    }
    check(rejected==-1,"custom 488 is reported as rejection");
    check(bt_async_http_status()==488,"preserves literal HTTP code");
    check(strstr(bt_async_request_path(),"/api/live?length=300&hz=50")!=NULL,"3DS-style request path recorded");
    check(strstr(bt_async_status_line(),"488 BrickOS custom rejection")!=NULL,"status line preserved");
    check(strstr(bt_async_reject_body(),"diagnostic: live length")!=NULL,"response body captured even in same TCP packet as headers");
    bt_async_cancel();

    /* A BrickOS 503 while waiting for its first live sample does not
     * require a fresh TCP socket. Reuse the connection for the next poll. */
    int pending_socket_count=socket_alloc_count;
    for(int i=0;i<3;i++){
        check(bt_async_begin(BT_NET_LIVE,NULL,&live,302,frame),"begin telemetry pending sequence");
        int pending=0;
        for(int j=0;j<500&&pending==0;j++){pending=bt_async_poll(++frame);usleep(300);}
        check(pending==(i<2?-1:1),"two 503 pending samples followed by valid telemetry");
        if(i<2){
            check(bt_async_http_status()==503,"capture original BrickOS 503 status");
            check(strstr(bt_async_reject_body(),"Waiting for first ECU telemetry sample")!=NULL,
                  "capture ESP32 sample-pending error body");
        }
        bt_async_cancel();
    }
    check(socket_alloc_count-pending_socket_count<=1,
          "503 retries did not reopen fresh sockets");
    /* No response must not hang a single poll call. Simulate ten seconds
     * of VBlank frames: a request must expire, with a specific error. */
    check(bt_async_begin(BT_NET_STATUS,NULL,NULL,0,frame),"start delayed status");
    int timeout=0;
    for(int i=0;i<620;i++){
        timeout=bt_async_poll(++frame);
        if(timeout!=0)break;
        usleep(250);
    }
    check(timeout==-1,"silent response has bounded timeout");
    check(strstr(bt_async_error(),"TEN SECONDS")!=NULL,"timeout error is diagnostic");
    bt_async_cancel();
    /* Repeated status refreshes must not exhaust DSWiFi's small socket pool.
     * This catches the v0.4 close()-instead-of-closesocket() regression. */
    int start_sockets=socket_alloc_count;
    for(int i=0;i<24;i++){
        check(bt_async_begin(BT_NET_STATUS,NULL,NULL,0,frame),"repeat status request");
        run_to_finish("repeat status request",500);
        bt_async_cancel();
    }
    check(socket_alloc_count-start_sockets<=3,
          "reuse HTTP connection for repeated requests, not 24 new DSWiFi sockets");
    bt_async_forget_connection();
    check(socket_close_count==socket_alloc_count,
          "all DSWiFi sockets released at shutdown, including persistent connection");
    check(nonblock_ioctl_count==socket_alloc_count,"every new socket configured with FIONBIO");
    puts("PASS: checked SD metadata cache, stale INI rejection, persistent HTTP, E12 handling");
    return 0;
}
