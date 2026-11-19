#include "wire.h"
#include "storage.h"
#include "signature.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <math.h>

/* devkitPro DSWifi owns its socket descriptors. newlib close() closes file
 * descriptors, not DSWifi sockets, and exhausts the socket pool over time.
 * Native host tests use POSIX close(); NDS ARM9 always uses closesocket(). */
#if defined(ARM9)
extern int closesocket(int socket_fd);
#define bts_socket_close closesocket
#else
#define bts_socket_close close
#endif

/* Synchronous protected writes execute only on the UI thread. Their nested
 * HTTP stack used >14 KiB of local arrays (JSON, network packets and verify
 * readback), easily exhausting the DSi ARM9 stack. One shared static set
 * eliminates that stack hazard; the async live client is cancelled before
 * the protected synchronous write path is invoked. */
static char sync_http_header[4096];
static uint8_t sync_http_pending[1024],sync_http_recv[2048];
static uint8_t sync_field_raw[4096],sync_write_verify[8192],sync_get_live_buf[BTS_LIVE_CAP+32];
static char sync_write_status[4096];
static BtProgressFn progress_fn=NULL;
void bt_set_progress(BtProgressFn callback){progress_fn=callback;}
/* One short-lived socket per request. The ESP32 endpoints are identical to
 * the v62 3DS client's endpoints; no hypothetical binary protocol is used. */
static int send_all(int fd,const char*b,size_t n) {
    for(size_t done=0;done<n;){
        int k=(int)send(fd,b+done,n-done,0);
        if(k<=0)return -1;
        done+=(size_t)k;
    }return 0;
}
static int open_socket(void){
    int s=socket(AF_INET,SOCK_STREAM,0);if(s<0)return -1;
    struct sockaddr_in addr;memset(&addr,0,sizeof(addr));
    addr.sin_family=AF_INET;addr.sin_port=htons(BTS_ESP_PORT);
    if(inet_aton(BTS_ESP_HOST,&addr.sin_addr)==0){bts_socket_close(s);return -1;}
    /* Avoid a UI that sits forever on dead Wi-Fi. */
    struct timeval tv={.tv_sec=1,.tv_usec=0};
    setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
    setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));
    if(connect(s,(struct sockaddr*)&addr,sizeof(addr))<0){bts_socket_close(s);return -1;}
    return s;
}
static bool deliver(BtChunkFn fn,void*user,const uint8_t *b,size_t n,size_t *received,size_t max){
    if(n>max-*received)return false;
    if(n&&fn&&!fn(b,n,user))return false;
    *received+=n;return true;
}
static const char *find_case(const char *hay,const char *needle){
    size_t n=strlen(needle);
    for(const char *p=hay;*p;p++)if(!strncasecmp(p,needle,n))return p;
    return NULL;
}
static bool has_token(const char *head,const char *needle){
    return find_case(head,needle)!=NULL;
}
/* HTTP/1.1 content-length and chunked transfer decoding, including chunks
 * crossing recv boundaries. Never feed the INI parser incomplete headers or
 * raw chunk delimiters. No dynamic large HTTP buffer is allocated. */
BtHttpResult bt_http(const char *method,const char *path,const char *post_data,BtChunkFn fn,void*user,size_t maxbytes){
    BtHttpResult r={0,0,false};
    int s=open_socket();if(s<0)return r;
    size_t length=post_data?strlen(post_data):0;
    char req[500];
    int h=snprintf(req,sizeof(req),"%s %s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\nConnection: close\r\n%s%s%s\r\n",
      method,path,BTS_ESP_HOST,post_data?"Content-Type: application/json\r\n":"",
      post_data?"Content-Length: ":"",post_data?"":"");
    if(h<0||h>=(int)sizeof(req))goto cleanup;
    if(post_data){
        /* Replace trailing blank line with Content-Length and CRLF. */
        h=snprintf(req,sizeof(req),"%s %s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %lu\r\n\r\n",
           method,path,BTS_ESP_HOST,(unsigned long)length);
        if(h<0||h>=(int)sizeof(req))goto cleanup;
    }
    if(send_all(s,req,(size_t)h)<0||(post_data&&send_all(s,post_data,length)<0))goto cleanup;
    char *header=sync_http_header;size_t hpos=0;
    bool headers=false,chunked=false,peer_closed=false,failed=false,zero_chunk=false;
    long content_len=-1;size_t body_count=0,remaining=0;
    enum{CHUNK_SIZE,CHUNK_BYTES,CHUNK_CR,CHUNK_LF,CHUNK_DONE} state=CHUNK_SIZE;
    char size_line[40];int size_at=0;
    uint8_t *pending=sync_http_pending;size_t psize=0;
    size_t last_progress=0;
    for(int attempts=0;attempts<4000&&!failed&&!zero_chunk;attempts++){
        uint8_t *chunk=sync_http_recv;int got=(int)recv(s,chunk,2048,0);
        if(got==0){peer_closed=true;break;}
        if(got<0){failed=true;break;}
        for(int i=0;i<got&&!failed&&!zero_chunk;i++){
            uint8_t c=chunk[i];
            if(!headers){
                if(hpos+1>=4096){failed=true;break;}
                header[hpos++]=(char)c;header[hpos]=0;
                if(hpos<4||memcmp(header+hpos-4,"\r\n\r\n",4))continue;
                headers=true;
                if(sscanf(header,"HTTP/%*s %d",&r.code)!=1){failed=true;break;}
                const char *cl=find_case(header,"Content-Length:");
                if(cl){content_len=strtol(cl+15,NULL,10);if(content_len<0||(size_t)content_len>maxbytes){failed=true;break;}}
                chunked=has_token(header,"Transfer-Encoding: chunked");
                continue;
            }
            if(r.code!=200){ /* Still drain a bounded error body. */
                if(++body_count>8192)failed=true;
                continue;
            }
            if(chunked){
                if(state==CHUNK_SIZE){
                    if(c=='\r')continue;
                    if(c=='\n'){
                        size_line[size_at]=0;char *end=NULL;
                        unsigned long n=strtoul(size_line,&end,16);
                        if(end==size_line||(*end&&*end!=';')||n>maxbytes-r.received){failed=true;break;}
                        remaining=(size_t)n;size_at=0;
                        if(!remaining){state=CHUNK_DONE;zero_chunk=true;}
                        else state=CHUNK_BYTES;
                    }else if(size_at<30)size_line[size_at++]=(char)c;
                    else failed=true;
                }else if(state==CHUNK_BYTES){
                    pending[psize++]=c;remaining--;
                    if(psize==1024){
                        if(!deliver(fn,user,pending,psize,&r.received,maxbytes)){failed=true;break;}
                        psize=0;
                    }
                    if(!remaining)state=CHUNK_CR;
                }else if(state==CHUNK_CR){if(c!='\r')failed=true;else state=CHUNK_LF;}
                else if(state==CHUNK_LF){if(c!='\n')failed=true;else state=CHUNK_SIZE;}
            }else if(content_len<0||(long)body_count<content_len){
                pending[psize++]=c;++body_count;
                if(psize==1024){
                    if(!deliver(fn,user,pending,psize,&r.received,maxbytes)){failed=true;break;}
                    psize=0;
                }
            }
            if(!chunked&&content_len>=0&&(long)body_count>=content_len)break;
        }
        if(progress_fn&&r.received-last_progress>=32768){last_progress=r.received;progress_fn(r.received);}
        if(!chunked&&headers&&content_len>=0&&(long)body_count>=content_len)break;
    }
    if(!failed&&psize&&!deliver(fn,user,pending,psize,&r.received,maxbytes))failed=true;
    r.complete=!failed&&headers&&r.code==200&&(chunked?zero_chunk:
       content_len>=0?(long)body_count==content_len:peer_closed);
cleanup:
    bts_socket_close(s);return r;
}
typedef struct{uint8_t*dst;size_t size,pos;} Buffer;
static bool copy_callback(const uint8_t*b,size_t n,void*ud){
    Buffer*v=(Buffer*)ud;
    if(n>v->size-v->pos)return false;
    memcpy(v->dst+v->pos,b,n);v->pos+=n;return true;
}
bool bt_get_status(char *json,size_t cap){
    if(cap<2)return false;
    Buffer b={(uint8_t*)json,cap-1,0};
    BtHttpResult r=bt_http("GET","/api/status",NULL,copy_callback,&b,cap-1);
    json[b.pos]=0;return r.complete;
}
static bool ini_callback(const uint8_t*b,size_t n,void*user){bt_ini_feed((BtDefinition*)user,b,n);return true;}
bool bt_get_ini(BtDefinition*d){
    bt_ini_reset(d);
    BtHttpResult r=bt_http("GET","/api/ini/file",NULL,ini_callback,d,2800000);
    return r.complete&&bt_ini_finish(d);
}
bool bt_get_live(BtLive *live,int length){
    if(length<4||length>BTS_LIVE_CAP)return false;
    char path[80];snprintf(path,sizeof(path),"/api/live?length=%d&hz=50",length);
    uint8_t *response=sync_get_live_buf;Buffer b={response,BTS_LIVE_CAP+32,0};
    BtHttpResult r=bt_http("GET",path,NULL,copy_callback,&b,BTS_LIVE_CAP+32);
    return r.complete&&bt_live_decode(live,response,b.pos);
}
static const char *key_at(const char*json,const char*key){
    char term[88];snprintf(term,sizeof(term),"\"%s\"",key);
    const char *p=strstr(json,term);
    if(!p)return NULL;
    p=strchr(p+strlen(term),':');
    if(!p)return NULL;
    p++;while(isspace((unsigned char)*p))p++;
    return p;
}
bool bt_json_bool(const char*json,const char*key,bool*out){
    const char*p=key_at(json,key);if(!p)return false;
    if(!strncmp(p,"true",4)){*out=true;return true;}
    if(!strncmp(p,"false",5)){*out=false;return true;}
    return false;
}
bool bt_json_string(const char*json,const char*key,char*out,size_t cap){
    const char*p=key_at(json,key);if(!p||*p!='"'||cap<1)return false;
    p++;size_t at=0;
    while(*p&&*p!='"'){
        if(*p=='\\'&&p[1])p++;
        if(at+1<cap)out[at++]=*p;
        p++;
    }
    out[at]=0;return *p=='"';
}
static bool hex_reply(const char*json,uint8_t *dst,int expected){
    const char*p=key_at(json,"hex");if(!p||*p!='"')return false;
    p++;for(int i=0;i<expected;i++){
        if(!isxdigit((unsigned char)p[0])||!isxdigit((unsigned char)p[1]))return false;
        char two[3]={p[0],p[1],0};dst[i]=(uint8_t)strtoul(two,NULL,16);p+=2;
    }
    return *p=='"';
}
static bool ts_read_page(const BtDefinition*d,int page,int offset,uint8_t*b,int length){
    if(page<1||page>64||offset<0||length<1||offset+length>65536)return false;
    int id=page<=d->page_count?d->page_ids[page-1]:(page-1)<<8;
    if(id<0||id>65535)return false;
    static const char h[]="0123456789abcdef";
    for(int at=0;at<length;){
        int count=length-at;if(count>256)count=256;
        unsigned atoff=(unsigned)(offset+at);
        uint8_t arg[6]={(uint8_t)id,(uint8_t)(id>>8),(uint8_t)atoff,(uint8_t)(atoff>>8),(uint8_t)count,(uint8_t)(count>>8)};
        char hex[13];for(int j=0;j<6;j++){hex[j*2]=h[arg[j]>>4];hex[j*2+1]=h[arg[j]&15];}hex[12]=0;
        char json[256];snprintf(json,sizeof(json),"{\"cmd\":\"R\",\"args\":\"%s\",\"timeout_ms\":6000}",hex);
        char reply[2048];Buffer result={(uint8_t*)reply,sizeof(reply)-1,0};
        BtHttpResult r=bt_http("POST","/api/ts",json,copy_callback,&result,sizeof(reply)-1);
        reply[result.pos]=0;
        const char *p=key_at(reply,"status");
        if(!r.complete||!p)return false;
        char *end=NULL;
        long response_status=strtol(p,&end,10);
        if(end==p||response_status!=0)return false;
        while(*end==' '||*end=='\t')end++;
        if(*end!=','&&*end!='}')return false;
        if(!hex_reply(reply,b+at,count))return false;
        at+=count;
    }
    return true;
}
bool bt_read_field(const BtDefinition *d,const BtField*f,double*values,int capacity){
    if(!d||!f||!f->scaled||!values)return false;
    int count=f->width*f->height,elem=bt_type_size(f->type);
    if(count<1||count>capacity||count*elem>4096||elem<1)return false;
    uint8_t *raw=sync_field_raw;
    if(!ts_read_page(d,f->page,f->offset,raw,count*elem))return false;
    /* bt_field_value() expects offsets relative to a full ECU page. */
    BtField copy=*f;copy.offset=0;
    for(int i=0;i<count;i++)if(!bt_field_value(d,&copy,raw,count*elem,i,&values[i]))return false;
    return true;
}

/* Full v62 BrickOS write contract: fresh session, independent stopped RPM,
 * complete ESP32 backup, compare-before-write, short-lived authorization,
 * verified C chunks, explicit B and post-burn readback. Never retry an
 * uncertain write. Only exposed through a separate two-stage user action. */
bool bt_read_raw(const BtDefinition*d,const BtField*f,uint8_t*dest,int cap){
    if(!d||!f||!dest)return false;
    int size=bt_type_size(f->type),length=size*f->width*f->height;
    return size>0&&length>0&&length<=cap&&ts_read_page(d,f->page,f->offset,dest,length);
}
static bool bt_reason(char*dst,size_t n,const char*msg){
    if(dst&&n)snprintf(dst,n,"%s",msg);
    return false;
}
static bool bt_num(const char*json,const char*key,double*out){
    const char*p=key_at(json,key);if(!p)return false;
    char *end=NULL;double v=strtod(p,&end);
    if(end==p||!isfinite(v)||(*end!=','&&*end!='}'&&!isspace((unsigned char)*end)))return false;
    *out=v;return true;
}
static bool json_escape(const char*src,char*dst,size_t cap){
    size_t at=0;
    if(!src||!*src||!cap)return false;
    for(const unsigned char*p=(const unsigned char*)src;*p;p++){
        if(*p<32||*p>126)return false;
        if(*p=='"'||*p=='\\'){
            if(at+2>=cap)return false;
            dst[at++]='\\';dst[at++]=(char)*p;
        }else{
            if(at+1>=cap)return false;
            dst[at++]=(char)*p;
        }
    }
    dst[at]=0;return true;
}
static bool post_json(const char*path,const char*body,char*reply,size_t cap){
    if(!reply||cap<2)return false;
    Buffer b={(uint8_t*)reply,cap-1,0};
    BtHttpResult r=bt_http("POST",path,body,copy_callback,&b,cap-1);
    reply[b.pos]=0;return r.complete;
}
static bool ok_reply(const char*json){bool ok=false;return bt_json_bool(json,"ok",&ok)&&ok;}
static void hex_encode(const uint8_t*b,int count,char*out){
    static const char h[]="0123456789abcdef";
    for(int i=0;i<count;i++){out[i*2]=h[b[i]>>4];out[i*2+1]=h[b[i]&15];}
    out[count*2]=0;
}
static bool ts_post(const char*command,const uint8_t*args,int count,
                    const char*signature,uint8_t*reply_bytes,int expected){
    if(count<0||count>134||expected<0||expected>256)return false;
    char hex[269];hex_encode(args,count,hex);
    char esc[320]="",request[800],reply[2048];
    bool signed_write=!strcmp(command,"C")||!strcmp(command,"B");
    if(signed_write&&!json_escape(signature,esc,sizeof(esc)))return false;
    int n=signed_write?
        snprintf(request,sizeof(request),
          "{\"cmd\":\"%s\",\"args\":\"%s\",\"timeout_ms\":9000,\"signature\":\"%s\"}",command,hex,esc):
        snprintf(request,sizeof(request),
          "{\"cmd\":\"%s\",\"args\":\"%s\",\"timeout_ms\":6000}",command,hex);
    if(n<1||(size_t)n>=sizeof(request)||!post_json("/api/ts",request,reply,sizeof(reply)))return false;
    const char *p=key_at(reply,"status");if(!p)return false;
    char*end=NULL;long status=strtol(p,&end,10);
    if(end==p||(*end!=','&&*end!='}'&&!isspace((unsigned char)*end))||
       (status!=0&&!(command[0]=='B'&&status==4)))return false;
    return expected==0||hex_reply(reply,reply_bytes,expected);
}
static bool engine_is_stopped(const BtDefinition*d){
    const BtField*f=bt_field(d,"RPMValue",true);
    if(!f||f->kind!=0||!f->scaled||f->offset<0)return false;
    int n=bt_type_size(f->type);
    if(n<1||n>8||f->offset+n>65536)return false;
    uint8_t args[4]={(uint8_t)f->offset,(uint8_t)(f->offset>>8),(uint8_t)n,(uint8_t)(n>>8)};
    uint8_t b[8]={0};double v=0;
    BtField copy=*f;copy.offset=0;
    return ts_post("O",args,4,NULL,b,n)&&bt_field_value(d,&copy,b,n,0,&v)&&fabs(v)<=0.1;
}
static bool backup_page(const BtDefinition*d,const BtField*f,const char*sig){
    int pg=f->page,id=pg<=d->page_count?d->page_ids[pg-1]:(pg-1)<<8;
    if(pg<1||pg>64||id<0||id>65535||d->page_sizes[pg-1]<=0)return false;
    char esc[320],body[570],reply[2048];
    if(!json_escape(sig,esc,sizeof(esc)))return false;
    int n=snprintf(body,sizeof(body),
       "{\"page\":%d,\"page_id\":%d,\"size\":%d,\"signature\":\"%s\"}",
       pg,id,d->page_sizes[pg-1],esc);
    if(n<1||(size_t)n>=sizeof(body)||!post_json("/api/backup",body,reply,sizeof(reply))||!ok_reply(reply))return false;
    char stored[40]="";double page=-1,length=-1,crc=-1;
    return bt_num(reply,"page",&page)&&bt_num(reply,"length",&length)&&bt_num(reply,"crc32",&crc)&&
           bt_json_string(reply,"stored",stored,sizeof(stored))&&page==pg&&
           length==d->page_sizes[pg-1]&&crc>=0&&crc<=4294967295.0&&!strcmp(stored,"ESP32_STORAGE");
}
static bool arm_ecu(const char*signature){
    char esc[320],body[480],reply[2048];
    if(!json_escape(signature,esc,sizeof(esc)))return false;
    int n=snprintf(body,sizeof(body),
       "{\"confirm\":\"BENCH_ONLY_ENGINE_STOPPED_BACKUP_SAVED\",\"signature\":\"%s\"}",esc);
    return n>0&&(size_t)n<sizeof(body)&&post_json("/api/arm",body,reply,sizeof(reply))&&ok_reply(reply);
}
static bool disarm_ecu(void){char reply[512];return post_json("/api/disarm","{}",reply,sizeof(reply))&&ok_reply(reply);}
static bool write_range(const BtDefinition*d,const BtField*f,const uint8_t*data,int begin,int length,const char*signature){
    int pg=f->page,id=pg<=d->page_count?d->page_ids[pg-1]:(pg-1)<<8;
    int off=f->offset+begin;
    if(id<0||id>65535||off<0||off+length>65536||length<1||length>128)return false;
    uint8_t args[134],check[128];
    args[0]=(uint8_t)id;args[1]=(uint8_t)(id>>8);
    args[2]=(uint8_t)off;args[3]=(uint8_t)(off>>8);
    args[4]=(uint8_t)length;args[5]=(uint8_t)(length>>8);
    memcpy(args+6,data+begin,(size_t)length);
    return ts_post("C",args,length+6,signature,NULL,0)&&
           ts_read_page(d,pg,off,check,length)&&!memcmp(check,data+begin,(size_t)length);
}
bool bt_commit_field(const BtDefinition*d,const BtField*f,const uint8_t*old_data,
                     const uint8_t*new_data,int length,char*reason,size_t reason_cap){
    if(!d||!d->valid||!f||!old_data||!new_data||length<1||length>8192||
       f->page<1||f->page>64||!memcmp(old_data,new_data,(size_t)length))
        return bt_reason(reason,reason_cap,"NO VALID STAGED CHANGE");
    int size=bt_type_size(f->type)*f->width*f->height,pg=f->page;
    if(size!=length||f->offset<0||d->page_sizes[pg-1]<1||
       f->offset+size>d->page_sizes[pg-1]||!d->page_writeable[pg-1]||!d->page_burnable[pg-1])
        return bt_reason(reason,reason_cap,"INI PAGE IS NOT SAFE TO WRITE");
    BtField rel=*f;rel.offset=0;
    for(int i=0;i<f->width*f->height;i++){
        double v=0;
        if(!bt_field_value(d,&rel,new_data,length,i,&v)||
           (f->limits_known&&(v<f->low-1e-8||v>f->high+1e-8)))
            return bt_reason(reason,reason_cap,"EDITED VALUE FAILS INI LIMIT CHECK");
    }
    char *status=sync_write_status;bool connected=false,compiled=false,ready=false,backup_ready=false;
    char signature[160]="";
    if(!bt_get_status(status,4096)||
       !bt_json_bool(status,"ecu_session_connected",&connected)||!connected||
       !bt_json_bool(status,"write_compiled",&compiled)||!compiled||
       !bt_json_bool(status,"write_access_ready",&ready)||!ready||
       !bt_json_bool(status,"backup_storage_ready",&backup_ready)||!backup_ready||
       !bt_json_string(status,"ecu_signature",signature,sizeof(signature))||
       !signature[0]||!bt_signature_matches(signature,d->signature))
        return bt_reason(reason,reason_cap,"SESSION / BACKUP / WRITE GATE FAILED");
    uint8_t *check=sync_write_verify;
    if(!ts_read_page(d,f->page,f->offset,check,length)||memcmp(check,old_data,(size_t)length))
        return bt_reason(reason,reason_cap,"ECU BASELINE CHANGED / RELOAD MAP");
    if(!engine_is_stopped(d))return bt_reason(reason,reason_cap,"ENGINE MUST BE STOPPED (FRESH RPM)");
    if(!backup_page(d,f,signature))return bt_reason(reason,reason_cap,"VERIFIED ESP BACKUP FAILED");
    if(!engine_is_stopped(d))return bt_reason(reason,reason_cap,"RPM RECHECK FAILED");
    bool armed=arm_ecu(signature),ok=armed;
    if(!armed){(void)disarm_ecu();return bt_reason(reason,reason_cap,"ECU ARM REQUEST REJECTED");}
    if(!engine_is_stopped(d))ok=false;
    for(int i=0;i<length&&ok;){
        if(old_data[i]==new_data[i]){i++;continue;}
        int start=i;while(i<length&&i-start<128&&old_data[i]!=new_data[i])i++;
        if(!engine_is_stopped(d)||!write_range(d,f,new_data,start,i-start,signature))ok=false;
    }
    if(ok&&!engine_is_stopped(d))ok=false;
    if(ok){int id=pg<=d->page_count?d->page_ids[pg-1]:(pg-1)<<8;
        if(id<0||id>65535)ok=false;
        else {uint8_t args[2]={(uint8_t)id,(uint8_t)(id>>8)};
            ok=ts_post("B",args,2,signature,NULL,0);}
    }
    if(ok)ok=ts_read_page(d,pg,f->offset,check,length)&&!memcmp(check,new_data,(size_t)length);
    bool disarmed=disarm_ecu();
    if(!ok)return bt_reason(reason,reason_cap,"WRITE / BURN UNCERTAIN: INSPECT ECU");
    if(!disarmed)return bt_reason(reason,reason_cap,"BURN VERIFIED / DISARM NOT CONFIRMED");
    if(reason&&reason_cap)snprintf(reason,reason_cap,"BURN VERIFIED / READBACK MATCHED");
    return true;
}

/* A single cooperative HTTP GET state machine for status, streamed INI,
 * and live data. It uses nonblocking sockets and a STRICT per-frame budget.
 * The old synchronous bt_http remains for explicit map read/write actions. */
/* DSWiFi sockets are not ordinary newlib file descriptors.  DSWiFi
 * exposes FIONBIO through <sys/ioctl.h>; fcntl(F_GETFL) fails with
 * ENOTSOCK (88) on the DSi, before any TCP/HTTP request can start. */
#include <sys/ioctl.h>

enum {NET_IDLE,NET_CONNECT,NET_SEND,NET_HEADER,NET_BODY,NET_DRAIN,NET_PARSE,NET_DONE,NET_FAILED,NET_ERROR_BODY};
enum {CH_SIZE,CH_DATA,CH_CR,CH_LF,CH_TRAIL};
typedef struct {
    int fd,phase,started_frame,last_io_frame,code;
    BtNetJob job;
    BtDefinition *definition;
    BtLive *live;
    FILE *reader;
    char ini_sig[160];
    char request[240], header[4096], error[96];
    char path[80], status_line[96], reject_body[96];
    size_t reject_body_len;
    size_t request_len,request_sent,header_len,received,expected,body_count,parsed_bytes;
    long content_length;
    bool chunked,finished_chunks, live_new;
    int chunk_phase,size_at;
    char size_line[40];
    size_t chunk_remaining;
    /* Receive in RAM, save in a DIFFERENT frame. No FAT call in recv loop. */
    uint8_t store[8192],block[8192];
    size_t stored,block_len,max_body;
} BtAsync;
static BtAsync async_net={.fd=-1,.phase=NET_IDLE};
/* HTTPD serves multiple HTTP/1.1 requests per connected client. Reusing one
 * idle TCP socket avoids the DS WiFi stack's small per-socket memory pool and
 * connection teardown churn during 20Hz gauge polling. Never pipeline. */
static int idle_http_fd=-1;
static bool response_keepalive=false;
static void close_idle_http(void){
    if(idle_http_fd>=0){bts_socket_close(idle_http_fd);idle_http_fd=-1;}
}
void bt_async_forget_connection(void){close_idle_http();}

static char expected_ini_sig[160]="";
static bool last_ini_signature_mismatch=false;
bool bt_async_signature_mismatch(void){return last_ini_signature_mismatch;}
void bt_async_expected_signature(const char *s){
    snprintf(expected_ini_sig,sizeof(expected_ini_sig),"%s",s?s:"");
}

static void async_close(void){
    if(async_net.fd>=0){bts_socket_close(async_net.fd);async_net.fd=-1;}
}
static void async_release_success(void){
    /* Reuse only complete fixed-length responses, including HTTP 503.
     * The ESP32 may send 503 while a telemetry sample is pending: closing
     * that socket and allocating another one repeatedly triggers DS E12. */
    if(response_keepalive && !async_net.chunked && async_net.content_length>=0 &&
       async_net.fd>=0 && async_net.job!=BT_NET_INI){
        close_idle_http();idle_http_fd=async_net.fd;async_net.fd=-1;
    }
    async_close();
}
void bt_async_cancel(void){
    async_close();
    if(async_net.reader){fclose(async_net.reader);async_net.reader=NULL;}
    if(async_net.job==BT_NET_INI)bt_sd_abort();
    response_keepalive=false;
    memset(&async_net,0,sizeof(async_net));
    async_net.fd=-1;
}
bool bt_async_busy(void){return (async_net.phase>=NET_CONNECT&&async_net.phase<=NET_PARSE)||async_net.phase==NET_ERROR_BODY;}
BtNetJob bt_async_job(void){return async_net.job;}
size_t bt_async_bytes(void){return async_net.received+async_net.block_len;}
size_t bt_async_saved_bytes(void){return async_net.job==BT_NET_INI?bt_sd_written():0;}
size_t bt_async_parsed_bytes(void){return async_net.parsed_bytes;}
size_t bt_async_total(void){return async_net.expected;}
int bt_async_http_status(void){return async_net.code;}
bool bt_async_live_is_new(void){return async_net.job==BT_NET_LIVE&&async_net.live_new;}
const char *bt_async_status_json(void){return async_net.job==BT_NET_STATUS?(const char*)async_net.store:"";}
const char *bt_async_error(void){return async_net.error;}
const char *bt_async_request_path(void){return async_net.path;}
const char *bt_async_status_line(void){return async_net.status_line;}
const char *bt_async_reject_body(void){return async_net.reject_body;}
const char *bt_async_phase(void){
    switch(async_net.phase){
        case NET_CONNECT:return "TCP CONNECT";
        case NET_SEND:return "HTTP REQUEST";
        case NET_ERROR_BODY:return "READING ESP ERROR";
        case NET_HEADER:return "WAITING HTTP HEADERS";
        case NET_BODY:return async_net.job==BT_NET_INI?"RECEIVING INI FROM ESP":
                              async_net.job==BT_NET_LIVE?"RECEIVING LIVE DATA":
                              async_net.job==BT_NET_READ?"READING ECU CALIBRATION":"RECEIVING ECU STATUS";
        case NET_DRAIN:return "FINISHING SD INI FILE";
        case NET_PARSE:return "PARSING INI FROM SD";
        case NET_DONE:return "TRANSFER COMPLETE";
        case NET_FAILED:return "NETWORK ERROR";
        default:return "IDLE";
    }
}
static int async_fail(const char*msg){
    if(async_net.code==200)snprintf(async_net.error,sizeof(async_net.error),"%s / GET 200 OK",msg);
    else snprintf(async_net.error,sizeof(async_net.error),"%s / HTTP %d",msg,async_net.code);
    async_net.phase=NET_FAILED;async_close();
    if(async_net.reader){fclose(async_net.reader);async_net.reader=NULL;}
    if(async_net.job==BT_NET_INI)bt_sd_abort();
    return -1;
}
/* Preserve the socket errno *before* snprintf/close can overwrite it. The
 * short message remains readable on the 256-pixel Link diagnostics display. */
static int async_fail_errno(const char*msg,int saved_errno){
    snprintf(async_net.error,sizeof(async_net.error),"%s E%d H%d",msg,saved_errno,async_net.code);
    async_net.phase=NET_FAILED;async_close();
    if(async_net.reader){fclose(async_net.reader);async_net.reader=NULL;}
    if(async_net.job==BT_NET_INI)bt_sd_abort();
    return -1;
}
static bool transient(int err){return err==EWOULDBLOCK||err==EAGAIN||err==EINPROGRESS||err==EALREADY||err==EINTR;}
bool bt_async_begin(BtNetJob job,BtDefinition*d,BtLive*live,int live_length,int frame){
    if(bt_async_busy()||job==BT_NET_NONE)return false;
    bt_async_cancel();
    if(job==BT_NET_INI)last_ini_signature_mismatch=false;
    async_net.job=job;async_net.definition=d;async_net.live=live;
    response_keepalive=false;
    async_net.started_frame=async_net.last_io_frame=frame;
    const char *path=job==BT_NET_STATUS?"/api/status":job==BT_NET_INI?"/api/ini/file":NULL;
    char live_path[72];
    if(job==BT_NET_INI){
        if(!d)return false;
        if(!bt_sd_begin()){async_fail(bt_sd_message());return false;}
        /* Preserve a parsed cached definition if socket() or connect()
         * fails (especially DSWiFi ENOMEM/E12). Reset only once the
         * transport has actually begun connecting. */
        async_net.max_body=2800000;
        snprintf(async_net.ini_sig,sizeof(async_net.ini_sig),"%s",expected_ini_sig);
    }
    else if(job==BT_NET_LIVE){
        if(!live||live_length<4||live_length>BTS_LIVE_CAP)return false;
        snprintf(live_path,sizeof(live_path),"/api/live?length=%d&hz=50",live_length);
        path=live_path;async_net.max_body=BTS_LIVE_CAP+32;
    }else if(job==BT_NET_READ){
        path="/api/ts";
        async_net.max_body=sizeof(async_net.store)-1;
    }else async_net.max_body=sizeof(async_net.store)-1;
    snprintf(async_net.path,sizeof(async_net.path),"%s",path);
    int n=snprintf(async_net.request,sizeof(async_net.request),
        "GET %s HTTP/1.1\r\nHost: %s\r\nAccept: */*\r\nConnection: keep-alive\r\n\r\n",
        path,BTS_ESP_HOST);
    if(n<1||(size_t)n>=sizeof(async_net.request))return false;
    async_net.request_len=(size_t)n;
    /* A completed preceding GET may have left us a reusable socket.
     * It already has FIONBIO configured and is connected to this ESP. */
    if(idle_http_fd>=0){
        async_net.fd=idle_http_fd;idle_http_fd=-1;
        if(job==BT_NET_INI)bt_ini_reset(d);
        async_net.phase=NET_SEND;
        return true;
    }
    async_net.fd=socket(AF_INET,SOCK_STREAM,0);
    if(async_net.fd<0){int saved=errno;async_fail_errno("SOCKET FAIL",saved);return false;}
    int nonblocking=1;
    if(ioctl(async_net.fd,FIONBIO,&nonblocking)<0){
        int saved=errno;async_fail_errno("SOCKET FIONBIO",saved);return false;
    }
    struct sockaddr_in addr={0};
    addr.sin_family=AF_INET;addr.sin_port=htons(BTS_ESP_PORT);
    if(!inet_aton(BTS_ESP_HOST,&addr.sin_addr)){
        async_fail("INVALID BRIDGE ADDRESS");return false;
    }
    int rc=connect(async_net.fd,(struct sockaddr*)&addr,sizeof(addr));
    if(rc==0){
        if(job==BT_NET_INI)bt_ini_reset(d);
        async_net.phase=NET_SEND;return true;
    }
    if(transient(errno)){
        if(job==BT_NET_INI)bt_ini_reset(d);
        async_net.phase=NET_CONNECT;return true;
    }
    {int saved=errno;async_fail_errno("TCP CONNECT",saved);return false;}
}
/* A map/scalar read uses the same nonblocking request scheduler and socket
 * as dashboard/status. The old synchronous bt_http allocated new sockets on
 * every user action, then blocked the ARM9 button loop until it finished. */
bool bt_async_begin_cal_read(int page_id,int offset,int count,int frame){
    if(page_id<0||page_id>65535||offset<0||offset>65535||
       count<1||count>256||offset+count>65536)return false;
    if(!bt_async_begin(BT_NET_READ,NULL,NULL,0,frame))return false;
    uint8_t args[6]={
        (uint8_t)page_id,(uint8_t)(page_id>>8),
        (uint8_t)offset,(uint8_t)(offset>>8),
        (uint8_t)count,(uint8_t)(count>>8)};
    char arg_hex[13];static const char digits[]="0123456789abcdef";
    for(int i=0;i<6;i++){
        arg_hex[2*i]=digits[args[i]>>4];
        arg_hex[2*i+1]=digits[args[i]&15];
    }
    arg_hex[12]=0;
    char payload[100];
    int p=snprintf(payload,sizeof(payload),
        "{\"cmd\":\"R\",\"args\":\"%s\",\"timeout_ms\":6000}",arg_hex);
    if(p<1||(size_t)p>=sizeof(payload)){bt_async_cancel();return false;}
    int n=snprintf(async_net.request,sizeof(async_net.request),
        "POST /api/ts HTTP/1.1\r\nHost: %s\r\nAccept: application/json\r\n"
        "Connection: keep-alive\r\nContent-Type: application/json\r\n"
        "Content-Length: %d\r\n\r\n%s",BTS_ESP_HOST,p,payload);
    if(n<1||(size_t)n>=sizeof(async_net.request)){
        bt_async_cancel();return false;
    }
    async_net.request_len=(size_t)n;
    return true;
}
bool bt_async_cal_read_result(uint8_t *dst,int count){
    if(async_net.job!=BT_NET_READ||async_net.phase!=NET_DONE||
       !dst||count<1||count>256||async_net.stored>=sizeof(async_net.store))return false;
    const char *json=(const char*)async_net.store;
    const char *p=key_at(json,"status");
    if(!p)return false;
    char *end=NULL;long status=strtol(p,&end,10);
    if(end==p||status!=0)return false;
    while(*end==' '||*end=='\t')end++;
    if(*end!=','&&*end!='}')return false;
    const char *length_value=key_at(json,"length");
    if(length_value){
        char *len_end=NULL;
        long response_length=strtol(length_value,&len_end,10);
        if(len_end==length_value||response_length!=count)return false;
    }
    return hex_reply(json,dst,count);
}

/* Drain at most 2KB of RAM data to the SD filesystem on one UI frame.
 * On a DSi, interleaving FAT writes with DSWiFi's recv() byte-loop can
 * stall both network and touch. Only bt_async_poll() invokes SD I/O. */
static int async_flush_block(void){
    if(!async_net.block_len)return 0;
    size_t count=async_net.block_len;
    if(count>2048)count=2048;
    if(async_net.job==BT_NET_INI){
        if(!bt_sd_append(async_net.block,count))return async_fail(bt_sd_message());
    }else{
        if(count>async_net.max_body-async_net.stored)return async_fail("BODY TOO LARGE");
        memcpy(async_net.store+async_net.stored,async_net.block,count);
        async_net.stored+=count;
    }
    async_net.received+=count;
    async_net.block_len-=count;
    if(async_net.block_len)memmove(async_net.block,async_net.block+count,async_net.block_len);
    return 0;
}
static int async_byte(uint8_t b){
    if(async_net.received+async_net.block_len>=async_net.max_body)
        return async_fail("RESPONSE EXCEEDS LIMIT");
    if(async_net.block_len==sizeof(async_net.block))
        return async_fail("SD RX QUEUE FULL");
    async_net.block[async_net.block_len++]=b;
    return 0;
}
static int async_complete_sd(void){
    if(async_net.block_len)return 0;
    if(!bt_sd_end())return async_fail(bt_sd_message());
    async_net.reader=bt_sd_open_pending();
    if(!async_net.reader)return async_fail("SD REOPEN INI FAILED");
    /* Verify that the filesystem really committed every received byte.
     * fwrite() alone can appear successful while data is still buffered. */
    if(fseek(async_net.reader,0,SEEK_END)!=0)
        return async_fail("SD INI SIZE CHECK FAILED");
    long actual_size=ftell(async_net.reader);
    if(actual_size<0||(size_t)actual_size!=async_net.received)
        return async_fail("SD INI SIZE DOES NOT MATCH RX");
    if(fseek(async_net.reader,0,SEEK_SET)!=0)
        return async_fail("SD INI REWIND FAILED");
    async_net.phase=NET_PARSE;
    return 0;
}
static int async_done(void){
    if(async_net.content_length>=0&&!async_net.chunked&&
       (size_t)async_net.content_length!=async_net.body_count)
        return async_fail("INCOMPLETE HTTP BODY");
    if(async_net.job==BT_NET_INI){
        /* Socket is complete. Save remaining buffered bytes and close the
         * temporary file in later frames before attempting to parse it. */
        async_close();async_net.phase=NET_DRAIN;
        return 0;
    }
    while(async_net.block_len)if(async_flush_block()<0)return -1;
    if(async_net.job==BT_NET_STATUS||async_net.job==BT_NET_READ){
        async_net.store[async_net.stored]=0;
        if(async_net.stored<2)return async_fail("EMPTY JSON RESPONSE");
    }
    if(async_net.job==BT_NET_LIVE&&!bt_live_decode_http(async_net.live,async_net.store,async_net.stored,&async_net.live_new))
        return async_fail("LIVE FRAME FORMAT INVALID");
    async_release_success();async_net.phase=NET_DONE;return 1;
}
static int async_header_byte(uint8_t c){
    if(async_net.header_len+1>=sizeof(async_net.header))return async_fail("HTTP HEADERS TOO LONG");
    size_t pos=async_net.header_len++;
    async_net.header[pos]=(char)c;async_net.header[pos+1]=0;
    if(pos<3||memcmp(async_net.header+pos-3,"\r\n\r\n",4))return 0;
    const char *endline=strstr(async_net.header,"\r\n");
    if(endline){
        size_t ln=(size_t)(endline-async_net.header);
        if(ln>=sizeof(async_net.status_line))ln=sizeof(async_net.status_line)-1;
        memcpy(async_net.status_line,async_net.header,ln);
        async_net.status_line[ln]=0;
    }
    /* Do not infer a server status from the app's display font. Preserve the
     * literal HTTP status line and the error body for SD diagnostics. */
    if(sscanf(async_net.status_line,"HTTP/%*s %d",&async_net.code)!=1)
        return async_fail("INVALID HTTP STATUS LINE");
    response_keepalive=!has_token(async_net.header,"Connection: close") &&
      !strncasecmp(async_net.status_line,"HTTP/1.1",8);
    if(async_net.code!=200){
        async_net.content_length=-1;
        const char *cl=find_case(async_net.header,"Content-Length:");
        if(cl){char *end=NULL;long n=strtol(cl+15,&end,10);
            if(end!=cl+15 && n>=0)async_net.content_length=n;}
        async_net.phase=NET_ERROR_BODY;
        if(async_net.content_length==0){
            snprintf(async_net.error,sizeof(async_net.error),"ESP32 REJECTED REQUEST / HTTP %d",async_net.code);
            async_release_success();async_net.phase=NET_FAILED;return -1;
        }
        return 0;
    }
    async_net.chunked=has_token(async_net.header,"Transfer-Encoding: chunked");
    /* Server may decline keepalive even when requested. */
    async_net.content_length=-1;
    const char*cl=find_case(async_net.header,"Content-Length:");
    if(cl&&!async_net.chunked){
        char *end=NULL;
        long n=strtol(cl+15,&end,10);
        if(end==cl+15||n<0||(size_t)n>async_net.max_body)return async_fail("BAD CONTENT LENGTH");
        async_net.content_length=n;async_net.expected=(size_t)n;
    }
    async_net.phase=NET_BODY;async_net.chunk_phase=CH_SIZE;
    if(async_net.content_length==0)return async_done();
    return 0;
}
static int async_reject_byte(uint8_t c){
    BtAsync *a=&async_net;
    if(a->reject_body_len+1<sizeof(a->reject_body)){
        a->reject_body[a->reject_body_len++]=(c>=32&&c<=126)?(char)c:' ';
        a->reject_body[a->reject_body_len]=0;
    }
    a->body_count++;
    if(a->body_count>=256)return async_fail("ESP32 ERROR BODY TOO LONG");
    if(a->content_length>=0&&(long)a->body_count>=a->content_length){
        snprintf(a->error,sizeof(a->error),"ESP32 REJECTED REQUEST / HTTP %d",a->code);
        async_release_success();a->phase=NET_FAILED;return -1;
    }
    return 0;
}
static int async_body_byte(uint8_t c){
    if(!async_net.chunked){
        if(async_byte(c)<0)return -1;
        async_net.body_count++;
        if(async_net.content_length>=0&&(long)async_net.body_count==async_net.content_length)return async_done();
        return 0;
    }
    switch(async_net.chunk_phase){
    case CH_SIZE:
        if(c=='\r')return 0;
        if(c=='\n'){
            async_net.size_line[async_net.size_at]=0;
            char *end=NULL;
            unsigned long n=strtoul(async_net.size_line,&end,16);
            if(end==async_net.size_line||(*end&&*end!=';')||n>async_net.max_body-bt_async_bytes())
                return async_fail("MALFORMED CHUNK SIZE");
            async_net.size_at=0;async_net.chunk_remaining=(size_t)n;
            if(!n)return async_done(); /* terminal 0 chunk */
            async_net.chunk_phase=CH_DATA;
        }else if(async_net.size_at<(int)sizeof(async_net.size_line)-1)
            async_net.size_line[async_net.size_at++]=(char)c;
        else return async_fail("CHUNK HEADER TOO LONG");
        break;
    case CH_DATA:
        if(async_byte(c)<0)return -1;
        if(!--async_net.chunk_remaining)async_net.chunk_phase=CH_CR;
        break;
    case CH_CR:
        if(c!='\r')return async_fail("CHUNK CR MISSING");
        async_net.chunk_phase=CH_LF;break;
    case CH_LF:
        if(c!='\n')return async_fail("CHUNK LF MISSING");
        async_net.chunk_phase=CH_SIZE;break;
    default:return async_fail("INVALID CHUNK STATE");
    }
    return 0;
}
int bt_async_poll(int frame){
    BtAsync *a=&async_net;
    if(a->phase==NET_PARSE){
        /* Parsing off the SD in bounded chunks keeps UI and touch responsive. */
        uint8_t bytes[1024];
        size_t n=fread(bytes,1,sizeof(bytes),a->reader);
        if(n){bt_ini_feed(a->definition,bytes,n);a->parsed_bytes+=n;return 0;}
        bool error=ferror(a->reader)!=0;
        fclose(a->reader);a->reader=NULL;
        if(error)return async_fail("SD INI READ FAILED");
        if(!bt_ini_finish(a->definition))
            return async_fail(a->definition->out_of_memory?"INI PARSER OUT OF RAM":
                a->definition->truncated?"INI DEFINITIONS EXCEED LIMIT":
                "ECU INI PARSE INVALID");
        /* Match the 3DS v62 decode_ini() acceptance flow exactly:
         * complete HTTP 200 -> successful INI parse -> cache on SD.
         * A different display/signature name is not a download failure.
         * Keep the strings for diagnostics, but DO NOT use them as a
         * prerequisite to accepting the definition. Independent guards
         * in bt_commit_field() still check ECU identity before any write. */
        bt_sd_signature_report(a->ini_sig,a->definition->signature,a->code);
        if(!bt_sd_commit())return async_fail(bt_sd_message());
        a->phase=NET_DONE;return 1;
    }
    if(a->phase==NET_DRAIN){
        if(a->block_len){if(async_flush_block()<0)return -1;return 0;}
        return async_complete_sd();
    }
    if(a->phase==NET_DONE)return 1;
    if(a->phase==NET_FAILED)return -1;
    if(!bt_async_busy())return 0;
    if(frame-a->last_io_frame>600)return async_fail("NO DATA FOR TEN SECONDS");
    if(a->phase==NET_CONNECT||a->phase==NET_SEND){
        /* Do not call newlib select(): DSWiFi owns the socket descriptor,
         * not the host/newlib FD table. A nonblocking send() acts as a
         * connect-completion probe and is retried once per frame while TCP
         * is still establishing. The existing 600-frame timeout applies. */
        int n=(int)send(a->fd,a->request+a->request_sent,a->request_len-a->request_sent,0);
        if(n<0){
            int saved=errno;
            if(transient(saved)||(a->phase==NET_CONNECT&&saved==ENOTCONN))return 0;
            return async_fail_errno(a->phase==NET_CONNECT?"TCP SEND":"HTTP SEND",saved);
        }
        if(n==0)return async_fail("SOCKET CLOSED WHILE SENDING");
        a->phase=NET_SEND;
        a->request_sent+=(size_t)n;a->last_io_frame=frame;
        if(a->request_sent==a->request_len)a->phase=NET_HEADER;
        return 0;
    }
    /* Backpressure: write queued bytes to SD before receiving another
     * packet. SD writes never happen while decoding one incoming packet. */
    if(a->block_len){if(async_flush_block()<0)return -1;return 0;}
    uint8_t recvbuf[2048];
    int n=(int)recv(a->fd,recvbuf,sizeof(recvbuf),0);
    if(n<0){if(transient(errno))return 0;return async_fail_errno("HTTP RECV",errno);}
    if(n==0){
        if(a->phase==NET_ERROR_BODY)return async_fail("ESP32 REJECTED REQUEST");
        if(a->phase==NET_BODY&&!a->chunked&&a->content_length<0)return async_done();
        return async_fail("SOCKET CLOSED BEFORE RESPONSE ENDED");
    }
    a->last_io_frame=frame;
    for(int i=0;i<n;i++){
        if(a->phase!=NET_HEADER && a->phase!=NET_BODY && a->phase!=NET_ERROR_BODY)break;
        int r=a->phase==NET_HEADER?async_header_byte(recvbuf[i]):
              a->phase==NET_ERROR_BODY?async_reject_byte(recvbuf[i]):async_body_byte(recvbuf[i]);
        if(r)return r;
    }
    /* Data is queued; the next frame writes it to SD or the small reply buffer. */
    return 0;
}

/* Cached raw INI is parsed in small pieces; never malloc the whole file. */
static FILE *cache_reader=NULL;
static BtDefinition *cache_definition=NULL;
static size_t cache_bytes=0;
static bool cache_out_of_memory=false;
bool bt_cache_begin(BtDefinition *d){
    if(!d||cache_reader||!bt_sd_ready())return false;
    FILE *f=bt_sd_open_cached();if(!f)return false;
    bt_ini_reset(d);cache_definition=d;cache_reader=f;cache_bytes=0;cache_out_of_memory=false;
    return true;
}
int bt_cache_poll(void){
    if(!cache_reader)return -1;
    uint8_t data[1024];
    size_t n=fread(data,1,sizeof(data),cache_reader);
    if(n){bt_ini_feed(cache_definition,data,n);cache_bytes+=n;return 0;}
    bool failed=ferror(cache_reader)!=0;
    fclose(cache_reader);cache_reader=NULL;
    if(failed || !bt_ini_finish(cache_definition)){
        cache_out_of_memory=cache_definition->out_of_memory;
        bt_ini_reset(cache_definition);return -1;
    }
    return 1;
}
size_t bt_cache_bytes(void){return cache_bytes;}
bool bt_cache_out_of_memory(void){return cache_out_of_memory;}
void bt_cache_cancel(void){
    if(cache_reader){fclose(cache_reader);cache_reader=NULL;}
}
