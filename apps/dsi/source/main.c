/* Brick Tuner Suite DSi v2.1
 * Native two-screen UI, sourced from the original 3DS v62 INI/BrickOS contract.
 * A DSi cannot render v62's Citro3D visuals; this edition retains a recognizable
 * dashboard / TunerStudio category tree / live channels / map and graph workflow.
 * INI-driven map edits are staged locally; protected writes use the same
 * verified v62 BrickOS bench-only transaction. 3DS files are untouched.
 */
#include <nds.h>
#include <dswifi9.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "tuner.h"
#include "dashboard.h"
#include "wire.h"
#include "storage.h"
#include "signature.h"
#include "font.h"
#include "ui_art.h"
#include "ui_font.h"
#include "sfx.h"
#include "bts_audio_rules.h"

#define W 256
#define H 192
#define SLOTS 2
#define HISTORY 96
#define EXPECTED_AP "brick"
#define COL(r,g,b) (RGB15((r),(g),(b))|BIT(15))
static const uint16_t BG=COL(3,4,6), PANEL=COL(7,9,12), EDGE=COL(13,16,18),
    LITE=COL(11,13,16), WHITE=COL(29,29,26), MUTED=COL(18,20,20),
    HIGHLIGHT=COL(31,22,12), GREEN=COL(12,27,17), RED=COL(29,11,12),
    CYAN=COL(12,25,27), SHADOW=COL(2,3,4);
/* Render both screens in RAM, then present the completed images together.
 * The previous version drew its background clearing directly into active VRAM,
 * making entire screens visibly blink when network status changed. */
static uint16_t draw_top[W*H],draw_bottom[W*H];
static uint16_t *top=draw_top,*bottom=draw_bottom;
static uint16_t *lcd_top,*lcd_bottom;
static BtDefinition def;
static BtLive live;
static BtTable table;
static BtMap scalar_map; /* synthetic INI scalar editor, never a fabricated ECU map */
static char actual_sig[160], firmware[60];
static char status_msg[92]="INITIALIZING WI-FI";
static char note[96]="";
static bool wifi_initialized=false,wifi_up=false,bridge_up=false,ecu_up=false,ini_ok=false,loading_ini=false;
/* ECU state is known only from a successful BrickOS /api/status response.
 * Network failure != ECU session disconnected. */
static bool ecu_status_known=false;
static bool want_ini=false,want_status=false;
static bool sd_ready=false,sd_attempted=false,loading_cache=false,cache_loaded=false;
static int assoc_state=ASSOCSTATUS_DISCONNECTED;
static int frame=0,live_frame=-9999,network_poll=0,wifi_attempt=0,ini_retry_frame=0;
static int live_poll=0;
/* DS/DSi DSWiFi has much less TCP memory than the 3DS network stack.
 * The old high-frequency socket churn exhausted its allocations (SOCKET FAIL E12).
 * A bounded, single-inflight transport reuses one HTTP/1.1 connection.
 * Target one distinct live sample every two VBlanks (30Hz). Actual measured
 * rate is shown on screen and depends on DSi Wi-Fi and ECU sample delivery. */
enum { BTS_STATUS_POLL_FRAMES=1200, BTS_LIVE_POLL_FRAMES=2,
       BTS_SOCKET_GAP_FRAMES=1, BTS_MAX_BACKOFF_FRAMES=960,
       BTS_IDLE_TIMEOUT_FRAMES=600 };
static int last_socket_begin_frame=-1000, network_resume_frame=0;
static unsigned network_failure_count=0;
/* Live polling has its own failure window, so a transient bad gauge frame
 * never triggers status-request storms or locks out the cached INI. */
static unsigned live_failure_streak=0;
static int live_resume_frame=0;
static char last_net_error[96]="";
static char last_failed_path[80]="";
static char last_response_line[96]="";
static char last_reject_body[96]="";
static BtNetJob last_failed_job=BT_NET_NONE;
static void network_success(BtNetJob job){
    if(job!=BT_NET_LIVE){network_failure_count=0;network_resume_frame=0;}
    if(job==BT_NET_LIVE){live_failure_streak=0;live_resume_frame=0;}
    /* A successful status probe must not erase a rejected LIVE request.
     * Keep diagnostics until the SAME endpoint succeeds or user retries. */
    if(job==last_failed_job){
        last_failed_job=BT_NET_NONE;
        last_net_error[0]=0;
        last_failed_path[0]=last_response_line[0]=last_reject_body[0]=0;
    }
}
static void network_failure(void){
    if(network_failure_count<10)network_failure_count++;
    int backoff=120;
    for(unsigned i=1;i<network_failure_count && backoff<BTS_MAX_BACKOFF_FRAMES;i++)
        backoff*=2;
    if(backoff>BTS_MAX_BACKOFF_FRAMES)backoff=BTS_MAX_BACKOFF_FRAMES;
    network_resume_frame=frame+backoff;
}

static bool ini_hash_mismatch=false;
static int last_http_status=0;
static int live_count=0,live_errors=0,measured_hz=0;
static int activity_frame=0;
static bool idle_dashboard=false;
static BtIdleGauges idle_gauges;
static size_t ini_progress=0,ini_disk_bytes=0,ini_parsed_bytes=0,ini_expected_bytes=0;
static char client_ip[24]="-",gateway_ip[24]="-";
static int page=0,list_pick=0,list_scroll=0,tune_cat=0,tune_focus=0,tune_group=-1;
static int home_pick=0;
/* 2/3/4 top gauges; default remains 2 AFR/VBAT. L/R only on HOME. */
static int gauge_layout=2;
static int sound_pick=0;
static int map_indices[BTS_MAX_MAPS],map_count=0,chosen_map=-1;
static int cx=0,cy=0,active_slot=0;
static char channel_names[SLOTS][64]={"AFRValue","VBatt"};
static double hist[SLOTS][HISTORY],low[SLOTS],high[SLOTS],sum[SLOTS];
static bool hist_valid[SLOTS][HISTORY];
static unsigned sample_count[SLOTS];
static int history_at=0;
static bool graph_stats=false;
static char dialog_name[64]="",dialog_label[76]="";
static char dialog_owner_stack[6][64],dialog_label_stack[6][76];
static int dialog_depth=0;
static int dialog_pick=0,dialog_scroll=0;
static bool dialog_value_ok=false;
static double dialog_value=0;
static char dialog_value_name[64]="";
static const double steps[]={0.1,1.0,5.0,10.0};
static int step_index=1;
static bool abandon_confirm=false;
static bool keypad_open=false;
static char keypad_input[24]="";
static int bit_choice_count=0;
static char bit_choice_labels[32][36];
static const char *keypad_keys[16]={
    "7","8","9","DEL", "4","5","6","CLR",
    "1","2","3","SIGN", ".","0","OK","BACK"
};

enum {P_HOME,P_TUNE,P_MAPS,P_LIVE,P_GRAPH,P_SOUNDS,P_LINK,P_COUNT};
static const char *tabs[P_COUNT]={"HOME","TUNE","MAPS","CHANNELS","GRAPH","SOUNDS","LINK"};
static const int home_pages[6]={P_TUNE,P_MAPS,P_LIVE,P_GRAPH,P_SOUNDS,P_LINK};
/* Icons converted from the original 3DS the 3DS art/tsicons collection. */
static const int home_icons[6]={5,11,12,11,6,7};
static const char *home_labels[6]={"TUNING SYSTEMS","ECU MAPS","LIVE CHANNELS","LIVE GRAPHS","SOUNDBOARD","CONNECTION"};
static void refresh_idle_gauges(void){
    bt_idle_select(&def,channel_names[0],channel_names[1],&idle_gauges);
}
static bool can_show_idle(void){
    /* Never obscure an editable map, an INI dialog or uncommitted changes. */
    return ini_ok && !loading_cache && !loading_ini && !table.valid &&
        !dialog_name[0] && (page==P_HOME || page==P_TUNE || page==P_MAPS);
}
static void update_idle(void){
    idle_dashboard=can_show_idle() && (frame-activity_frame)>=BTS_IDLE_TIMEOUT_FRAMES;
}
static const char *sound_labels[BTS_SOUND_COUNT]={
 "LAUNCH CONTROL","VVL","4 CYLINDER","CONFIG ERROR",
 "BOOST / NITROUS","LIMITER / ANTILAG","UNSAVED",
 "BURN START","BURN VERIFIED","CONNECTED",
 "COOLANT HOT","LAMBDA DISPLAY","UNDO","OVER 6 CYLINDERS"};

/* 16-bit bitmap renderer: all objects have fixed-pixel rounded corners.
 * No stretched sprite corners, 3D model or framebuffer-dependent masks. */
static void pix(uint16_t*d,int x,int y,uint16_t c){if(x>=0&&y>=0&&x<W&&y<H)d[y*W+x]=c;}
static void fill(uint16_t*d,int x,int y,int w,int h,uint16_t c){
    if(x<0){w+=x;x=0;}if(y<0){h+=y;y=0;}
    if(x+w>W)w=W-x;
    if(y+h>H)h=H-y;
    if(w<=0||h<=0)return;
    for(int r=0;r<h;r++){uint16_t*p=d+(y+r)*W+x;for(int i=0;i<w;i++)p[i]=c;}
}
static void round_fill(uint16_t*d,int x,int y,int w,int h,int r,uint16_t c){
    if(r<0)r=0;
    if(r>w/2)r=w/2;
    if(r>h/2)r=h/2;
    fill(d,x+r,y,w-2*r,h,c);fill(d,x,y+r,r,h-2*r,c);fill(d,x+w-r,y+r,r,h-2*r,c);
    for(int yy=0;yy<r;yy++)for(int xx=0;xx<r;xx++){
        int dx=r-xx-1,dy=r-yy-1;
        if(dx*dx+dy*dy<=r*r){pix(d,x+xx,y+yy,c);pix(d,x+w-1-xx,y+yy,c);
            pix(d,x+xx,y+h-1-yy,c);pix(d,x+w-1-xx,y+h-1-yy,c);}
    }
}
static void ln(uint16_t*d,int x0,int y0,int x1,int y1,uint16_t c){
    int dx=abs(x1-x0),dy=-abs(y1-y0),sx=x0<x1?1:-1,sy=y0<y1?1:-1,e=dx+dy;
    for(;;){pix(d,x0,y0,c);if(x0==x1&&y0==y1)break;
        int e2=e*2;if(e2>=dy){e+=dy;x0+=sx;}if(e2<=dx){e+=dx;y0+=sy;}}
}
/* One 9x14 bitmap font with a common baseline for ALL normal UI labels.
 * Exactly the same glyph rasterizer on both screens. */
static void glyph(uint16_t *d,int x,int y,unsigned char ch,uint16_t color,int scale){
    if(ch<32||ch>126)ch='?';
    for(int yy=0;yy<14;yy++){
        uint32_t row=bts_ui_font[ch-32][yy];
        for(int xx=0;xx<9;xx++){
            int a=(row>>(xx*2))&3;
            if(!a)continue;
            int px=x+xx*scale,py=y+yy*scale;
            if(px>=W||py>=H||px+scale<=0||py+scale<=0)continue;
            for(int ry=0;ry<scale;ry++)for(int rx=0;rx<scale;rx++){
                int ix=px+rx,iy=py+ry;
                if(ix<0||iy<0||ix>=W||iy>=H)continue;
                int pos=iy*W+ix;
                uint16_t prev=d[pos];
                int red=((prev&31)*(3-a)+(color&31)*a)/3;
                int green=(((prev>>5)&31)*(3-a)+((color>>5)&31)*a)/3;
                int blue=(((prev>>10)&31)*(3-a)+((color>>10)&31)*a)/3;
                d[pos]=(uint16_t)(0x8000|red|(green<<5)|(blue<<10));
            }
        }
    }
}
static void text(uint16_t*d,int x,int y,const char*s,uint16_t c,int limit){
    if(!s)return;
    for(int i=0;s[i]&&i<limit&&x+i*9<W;i++)
        glyph(d,x+i*9,y,(unsigned char)s[i],c,1);
}
static void right_text(uint16_t*d,int x,int y,const char*s,uint16_t c){int n=(int)strlen(s);text(d,x-n*9,y,s,c,n);}
static void nav_icon_mini(uint16_t*d,int which,int x,int y){
    if(which<0||which>=13)return;
    for(int yy=0;yy<13;yy++)for(int xx=0;xx<13;xx++){
        uint16_t c=bts_nav_art[which][(yy*BTS_ICON_SIZE/13)*BTS_ICON_SIZE+xx*BTS_ICON_SIZE/13];
        if(c)pix(d,x+xx,y+yy,c);
    }
}
static int category_icon(const char*name){
    /* These are the real art/tsicons/00..12.png indices, not fabricated
     * "fuel=2/spark=4" indexes. Match the 3DS' semantic icon placement. */
    if(bts_contains_ci(name,"fuel")||bts_contains_ci(name,"inject")||bts_contains_ci(name,"afr"))return 2;
    if(bts_contains_ci(name,"ignit")||bts_contains_ci(name,"spark")||bts_contains_ci(name,"coil"))return 1;
    if(bts_contains_ci(name,"trigger")||bts_contains_ci(name,"crank")||bts_contains_ci(name,"cam"))return 4;
    if(bts_contains_ci(name,"sensor")||bts_contains_ci(name,"analog")||bts_contains_ci(name,"input"))return 7;
    if(bts_contains_ci(name,"boost")||bts_contains_ci(name,"turbo"))return 2;
    if(bts_contains_ci(name,"throttle")||bts_contains_ci(name,"idle")||bts_contains_ci(name,"dbw"))return 3;
    if(bts_contains_ci(name,"map")||bts_contains_ci(name,"table")||bts_contains_ci(name,"curve"))return 11;
    if(bts_contains_ci(name,"graph")||bts_contains_ci(name,"gauge")||bts_contains_ci(name,"channel"))return 11;
    if(bts_contains_ci(name,"connect")||bts_contains_ci(name,"wifi")||bts_contains_ci(name,"can"))return 7;
    if(bts_contains_ci(name,"setting")||bts_contains_ci(name,"config"))return 5;
    return 3;
}
static void number(double v,char*b,int cap){
    if(!isfinite(v)){snprintf(b,cap,"--");return;}
    if(fabs(v)>=1000)snprintf(b,cap,"%.0f",v);
    else if(fabs(v)>=10)snprintf(b,cap,"%.1f",v);
    else snprintf(b,cap,"%.2f",v);
}
static void draw_header(const char*heading){
    fill(top,0,0,W,H,BG);fill(top,0,0,W,25,PANEL);fill(top,0,24,W,1,EDGE);
    text(top,9,8,"BRICK",HIGHLIGHT,6);text(top,52,8,"/",MUTED,1);
    text(top,67,8,heading,WHITE,24);
    round_fill(top,242,9,7,7,3,ini_ok?GREEN:(wifi_up?HIGHLIGHT:RED));
}
/* Render a recently received real gauge frame even when a separate status
 * request fails. Status failure is NOT proof the ECU disconnected. */
static bool fresh(void){return live.valid&&frame-live_frame<600&&ini_ok&&ecu_up&&wifi_up;}
static bool get_value(const char*name,double*value){
    if(!fresh())return false;
    const BtField*f=bt_field(&def,name,true);
    return f&&bt_field_value(&def,f,live.bytes,live.length,0,value);
}
static void clear_history(int slot){
    for(int i=0;i<HISTORY;i++)hist_valid[slot][i]=false;
    sample_count[slot]=0;sum[slot]=0;low[slot]=0;high[slot]=0;
}
static void invalidate(void){live.valid=false;for(int i=0;i<SLOTS;i++)clear_history(i);}
/* In v62 the downloaded INI is a UI definition, not a signature-gated
 * network response. The DSi must also show a successfully parsed definition
 * independently of a status/INI naming difference. Signature checking is
 * retained independently in bt_commit_field() before ECU writes. */
static void set_compat(void){
    if(ini_ok&&!def.valid)invalidate();
    ini_ok=def.valid;
}
static void draw_spark(int slot,int x,int y,int width,int height){
    int points=0;double mi=1e99,ma=-1e99;
    for(int j=0;j<HISTORY;j++)if(hist_valid[slot][j]){
        double v=hist[slot][j];if(v<mi)mi=v;if(v>ma)ma=v;points++;
    }
    if(points<2){fill(top,x,y+height/2,width,1,EDGE);return;}
    if(ma-mi<0.001){mi-=0.5;ma+=0.5;}
    for(int j=1;j<HISTORY;j++){
        int a=(history_at+j)%HISTORY,b=(a+1)%HISTORY;
        if(!hist_valid[slot][a]||!hist_valid[slot][b])continue;
        int ya=y+height-1-(int)((hist[slot][a]-mi)/(ma-mi)*(height-1));
        int yb=y+height-1-(int)((hist[slot][b]-mi)/(ma-mi)*(height-1));
        ln(top,x+(j-1)*(width-1)/(HISTORY-1),ya,x+j*(width-1)/(HISTORY-1),yb,slot==active_slot?HIGHLIGHT:CYAN);
    }
}
/* Compact gauge panels use the same antialiased font and native 3DS
 * INI-defined channel metadata. The additional slots come from the same
 * nonduplicate, prioritized set as the 3DS idle gauge deck. */
static const char *gauge_name(int index){
    if(index==0||index==1)return channel_names[index];
    int extra=index-2;
    if(extra>=0&&extra<idle_gauges.count){
        int at=idle_gauges.field_index[extra];
        if(at>=0&&at<def.channel_count)return def.channels[at].name;
    }
    return "NOT CONFIGURED";
}
static void compact_gauge(int index,int x,int y,int w,int h){
    round_fill(top,x+1,y+2,w,h,5,SHADOW);
    round_fill(top,x,y,w,h,5,index==active_slot?LITE:PANEL);
    fill(top,x+5,y+6,2,h-12,index==active_slot?HIGHLIGHT:EDGE);
    const char *name=gauge_name(index);
    char label[24];snprintf(label,sizeof(label),"%.20s",name);
    text(top,x+9,y+5,label,MUTED,(w-12)/9);
    double v=0;bool valid=get_value(name,&v);
    char buf[32];
    if(valid)number(v,buf,sizeof(buf));else strcpy(buf,"--");
    int scale=(w>=105&&h>=85)?3:2;
    int cw=9*scale,cap=(w-12)/cw;
    if(cap<1)cap=1;
    if((int)strlen(buf)>cap){if(valid)snprintf(buf,sizeof(buf),"%.0f",v);else strcpy(buf,"--");}
    if((int)strlen(buf)>cap)buf[cap]=0;
    int width=(int)strlen(buf)*cw;
    int baseline=y+(h>=85?43:24);
    for(int k=0;buf[k]&&k<cap;k++)glyph(top,x+(w-width)/2+k*cw,baseline,(unsigned char)buf[k],valid?WHITE:MUTED,scale);
    if(h>=85){
        const BtField *field=bt_field(&def,name,true);
        if(field&&field->unit[0])text(top,x+9,y+h-19,field->unit,MUTED,(w-16)/9);
    }
}
static void top_dashboard(void){
    draw_header("DASHBOARD");
    if(gauge_layout==2){
        compact_gauge(0,5,28,121,127);
        compact_gauge(1,130,28,121,127);
    }else if(gauge_layout==3){
        compact_gauge(0,4,28,81,127);
        compact_gauge(1,88,28,81,127);
        compact_gauge(2,172,28,80,127);
    }else{
        compact_gauge(0,5,27,120,63);
        compact_gauge(1,130,27,121,63);
        compact_gauge(2,5,93,120,63);
        compact_gauge(3,130,93,121,63);
    }
    char b[40];
    snprintf(b,sizeof(b),"LIVE %d HZ / TARGET 30",measured_hz);
    text(top,8,160,b,measured_hz>=30?GREEN:measured_hz?HIGHLIGHT:MUTED,26);
    snprintf(b,sizeof(b),"L/R: %d GAUGES  |  %s",gauge_layout,bridge_up?"LINK OK":"OFFLINE");
    text(top,8,176,b,bridge_up?GREEN:MUTED,27);
}
static void top_channels(void){
    draw_header("LIVE CHANNELS");
    if(!def.channel_count){text(top,12,44,"CONNECT ECU TO LOAD CHANNELS",MUTED,34);return;}
    if(list_pick<0||list_pick>=def.channel_count)return;
    const BtField*f=&def.channels[list_pick];
    text(top,13,38,f->name,HIGHLIGHT,33);
    double v=0;bool ok=get_value(f->name,&v);
    char b[70];if(ok)number(v,b,sizeof(b));else snprintf(b,sizeof(b),"--");
    text(top,13,62,b,ok?WHITE:MUTED,30);
    text(top,13,91,f->unit[0]?f->unit:"NO UNIT",MUTED,28);
    fill(top,13,112,230,1,EDGE);
    snprintf(b,sizeof(b),"INI OFFSET %d  /  %d BYTE",f->offset,bt_type_size(f->type));
    text(top,13,126,b,MUTED,34);
    snprintf(b,sizeof(b),"X: ASSIGN TO %s",active_slot==0?"LEFT":"RIGHT");
    text(top,13,148,b,WHITE,34);
    text(top,13,168,"L/R: SELECT DASHBOARD GAUGE",HIGHLIGHT,34);
}
static void build_map_index(void){
    refresh_idle_gauges();
    map_count=0;
    for(int i=0;i<def.map_count;i++){
        const BtField*f=bt_field(&def,def.maps[i].z,false);
        if(f&&f->scaled&&f->width>0&&f->height>0&&f->width*f->height<=BTS_MAP_CELLS)
            map_indices[map_count++]=i;
    }
}
static const BtMap* selected_map(void){
    if(chosen_map==BTS_MAX_MAPS)return &scalar_map;
    if(chosen_map>=0&&chosen_map<def.map_count)return &def.maps[chosen_map];
    return NULL;
}
static int map_from_row(int n){return n>=0&&n<map_count?map_indices[n]:-1;}
static int map_for_target(const char*name){
    if(!name||!*name)return -1;
    for(int j=0;j<map_count;j++){
        int i=map_indices[j];const BtMap*m=&def.maps[i];
        if(!strcasecmp(name,m->name)||(m->alt[0]&&!strcasecmp(name,m->alt)))return i;
    }
    return -1;
}
/* Trace only INI-declared live runtime axes. Never guess missing channels. */
static int trace_x=-1,trace_y=-1;
static bool trace_valid=false;
static int nearest_bin(double value,const double *bins,int n){
    if(!bins||n<1)return -1;
    int best=0;double distance=fabs(bins[0]-value);
    for(int i=1;i<n;i++){
        double d=fabs(bins[i]-value);
        if(d<distance){best=i;distance=d;}
    }
    return best;
}
static void map_live_trace(const BtMap*m){
    trace_valid=false;trace_x=trace_y=-1;
    if(!m||!table.valid||!table.has_x||!m->xchan[0])return;
    double x=0,y=0;
    if(!get_value(m->xchan,&x))return;
    int tx=nearest_bin(x,table.xbins,table.width);
    if(tx<0)return;
    int ty=0;
    if(table.height>1){
        if(!table.has_y||!m->ychan[0]||!get_value(m->ychan,&y))return;
        ty=nearest_bin(y,table.ybins,table.height);
        if(ty<0)return;
    }
    trace_x=tx;trace_y=ty;trace_valid=true;
}
static void map_top(void){
    draw_header(table.dirty?"MAP / STAGED CHANGES":"ECU CALIBRATION MAP");
    if(chosen_map<0||!table.valid){
        text(top,11,45,"REAL ECU DEFINITION",HIGHLIGHT,32);
        text(top,11,64,"MAPS AND CURVES",WHITE,32);
        text(top,11,105,"SELECT A MAP BELOW",MUTED,32);
        text(top,11,132,"A / TOUCH TO VIEW",MUTED,32);
        return;
    }
    const BtMap*m=selected_map();if(!m)return;
    map_live_trace(m);
    text(top,11,31,m->label,HIGHLIGHT,27);
    int cols=table.width<5?table.width:5,rows=table.height<5?table.height:5;
    int left=cx-cols/2,above=cy-rows/2;
    if(left<0)left=0;
    if(above<0)above=0;
    if(left+cols>table.width)left=table.width-cols;
    if(above+rows>table.height)above=table.height-rows;
    for(int yy=0;yy<rows;yy++)for(int xx=0;xx<cols;xx++){
        int tx=left+xx,ty=above+yy,px=10+xx*47,py=52+yy*22;
        bool selected=tx==cx&&ty==cy;
        bool traced=trace_valid&&tx==trace_x&&ty==trace_y;
        round_fill(top,px,py,44,19,3,traced?GREEN:selected?HIGHLIGHT:PANEL);
        char b[18];number(table.cells[ty*table.width+tx],b,sizeof(b));
        text(top,px+3,py+4,b,selected||traced?BG:WHITE,5);
    }
    char b[60];snprintf(b,sizeof(b),"%d X %d   CELL %d,%d",table.width,table.height,cx+1,cy+1);
    text(top,10,169,b,MUTED,33);
    if(trace_valid){
        snprintf(b,sizeof(b),"LIVE TRACE %d,%d",trace_x+1,trace_y+1);
        text(top,10,181,b,GREEN,30);
    }else text(top,10,181,"LIVE TRACE: AXES UNAVAILABLE",MUTED,30);
    if(table.dirty)text(top,10,156,"STAGED / NOT BURNT",HIGHLIGHT,27);
}
static int category_count(void){int n=0;for(int i=0;i<def.menu_count;i++)if(def.menus[i].group==0)n++;return n;}
static int category_at(int c){
    int seen=0;for(int i=0;i<def.menu_count;i++)if(def.menus[i].group==0){if(seen++==c)return i;}
    return -1;
}
static int category_end(int index){int i=index+1;while(i<def.menu_count&&def.menus[i].group!=0)i++;return i;}
static int tune_item_count(void){
    if(tune_group>=0){int n=0;for(int i=tune_group+1;i<def.menu_count&&def.menus[i].group==2;i++)n++;return n;}
    int start=category_at(tune_cat);if(start<0)return 0;
    int n=0;for(int i=start+1;i<category_end(start);i++)if(def.menus[i].group!=2)n++;
    return n;
}
static int tune_item_at(int row){
    if(row<0)return -1;
    if(tune_group>=0){for(int i=tune_group+1;i<def.menu_count&&def.menus[i].group==2;i++)if(row--==0)return i;return -1;}
    int start=category_at(tune_cat);if(start<0)return -1;
    for(int i=start+1;i<category_end(start);i++)if(def.menus[i].group!=2&&row--==0)return i;
    return -1;
}
static int dialog_count(void){
    int count=0;
    for(int i=0;i<def.ref_count;i++)if(!strcasecmp(def.refs[i].owner,dialog_name))count++;
    return count;
}
static void open_map(int id);
static int dialog_at(int row){
    if(row<0)return -1;
    for(int i=0;i<def.ref_count;i++)if(!strcasecmp(def.refs[i].owner,dialog_name)&&row--==0)return i;
    return -1;
}
static void open_dialog(const char*id,const char*label){
    if(dialog_name[0]){
        if(dialog_depth>=6){snprintf(note,sizeof(note),"MAX NESTED PANEL DEPTH");return;}
        snprintf(dialog_owner_stack[dialog_depth],sizeof(dialog_owner_stack[0]),"%s",dialog_name);
        snprintf(dialog_label_stack[dialog_depth],sizeof(dialog_label_stack[0]),"%s",dialog_label);
        dialog_depth++;
    }
    snprintf(dialog_name,sizeof(dialog_name),"%s",id);
    snprintf(dialog_label,sizeof(dialog_label),"%s",label);
    dialog_pick=dialog_scroll=0;dialog_value_ok=false;
}
static void dialog_action(void){
    int index=dialog_at(dialog_pick);if(index<0||!ini_ok)return;
    const BtDialogRef*r=&def.refs[index];
    if(r->kind==1){bts_sfx_play(bts_sound_for_entry(r->label,r->name),sd_ready);open_dialog(r->name,r->label);return;}
    int target_map=map_for_target(r->name);
    if(target_map>=0){open_map(target_map);return;}
    const BtField*f=bt_field(&def,r->name,false);
    dialog_value_ok=false;
    snprintf(dialog_value_name,sizeof(dialog_value_name),"%s",r->name);
    if(!f||!f->scaled||f->width*f->height<1||
       f->width*f->height>BTS_MAP_CELLS){
        snprintf(note,sizeof(note),"INI FIELD HAS NO NUMERIC ENCODING");return;
    }
    bit_choice_count=0;
    if(f->kind==2&&sd_ready)
        bit_choice_count=bt_bit_choices(bt_sd_cache_path(),f->name,bit_choice_labels,32);
    memset(&scalar_map,0,sizeof(scalar_map));
    snprintf(scalar_map.label,sizeof(scalar_map.label),"%s",r->label);
    snprintf(scalar_map.z,sizeof(scalar_map.z),"%s",f->name);
    open_map(BTS_MAX_MAPS);
}
static void top_dialog(void){
    draw_header("ECU SETTINGS / READ");
    text(top,10,35,dialog_label,HIGHLIGHT,33);
    int at=dialog_at(dialog_pick);
    if(at>=0){
        const BtDialogRef*r=&def.refs[at];
        text(top,10,65,r->label,WHITE,33);
        text(top,10,85,r->name,MUTED,33);
        const BtField*f=bt_field(&def,r->name,false);
        if(dialog_value_ok&&!strcasecmp(dialog_value_name,r->name)){
            char b[32];number(dialog_value,b,sizeof(b));
            text(top,10,109,b,HIGHLIGHT,32);
            if(f)text(top,10,127,f->unit,MUTED,32);
        }else text(top,10,112,"A: READ ECU SCALAR",MUTED,33);
    }
    fill(top,10,147,236,1,EDGE);
    text(top,10,158,"A: OPEN / EDIT SCALAR FIELD",MUTED,34);
    if(note[0])text(top,10,177,note,HIGHLIGHT,34);
}
static void top_tune(void){
    if(chosen_map>=0&&table.valid){map_top();return;}
    if(dialog_name[0]){top_dialog();return;}
    draw_header("TUNERSTUDIO TREE");
    int count=category_count();
    if(!count){text(top,12,46,"NO TUNING MENU IN ECU INI",MUTED,33);return;}
    int first=tune_cat>9?tune_cat-9:0;
    for(int i=first;i<count&&i<first+10;i++){
        int at=category_at(i),y=30+(i-first)*15;
        bool selected=i==tune_cat;
        if(selected)round_fill(top,6,y,244,14,3,PANEL);
        nav_icon_mini(top,category_icon(def.menus[at].label),11,y);
        text(top,30,y+1,def.menus[at].label,selected?HIGHLIGHT:WHITE,27);
    }
    if(tune_focus)text(top,9,182,"LOWER MENU ACTIVE",HIGHLIGHT,30);
}
static void top_graph(void){
    draw_header(graph_stats?"LIVE STATISTICS":"LIVE GRAPH");
    int slot=active_slot;const char*name=channel_names[slot];
    text(top,12,34,name,HIGHLIGHT,32);
    if(graph_stats){
        char v[32];
        if(sample_count[slot]){
            number(low[slot],v,sizeof(v));text(top,14,65,"MIN",MUTED,10);text(top,87,65,v,WHITE,18);
            number(high[slot],v,sizeof(v));text(top,14,95,"MAX",MUTED,10);text(top,87,95,v,WHITE,18);
            number(sum[slot]/sample_count[slot],v,sizeof(v));text(top,14,125,"AVG",MUTED,10);text(top,87,125,v,WHITE,18);
            snprintf(v,sizeof(v),"N=%u",sample_count[slot]);text(top,14,156,v,MUTED,24);
        }else text(top,14,79,"NO LIVE SAMPLES YET",MUTED,32);
    }else{
        for(int j=0;j<5;j++)ln(top,12,67+j*22,245,67+j*22,EDGE);
        draw_spark(slot,13,64,231,93);
        char b[40];double v=0;if(get_value(name,&v))number(v,b,sizeof(b));else strcpy(b,"--");
        text(top,13,168,b,WHITE,20);
        if(sample_count[slot]){
            number(high[slot],b,sizeof(b));right_text(top,243,46,b,MUTED);
            number(low[slot],b,sizeof(b));right_text(top,243,158,b,MUTED);
        }
    }
}
static void current_ip(void){
    struct in_addr gw={0},mask={0},dns1={0},dns2={0};
    struct in_addr ip=Wifi_GetIPInfo(&gw,&mask,&dns1,&dns2);
    snprintf(client_ip,sizeof(client_ip),"%s",inet_ntoa(ip));
    snprintf(gateway_ip,sizeof(gateway_ip),"%s",inet_ntoa(gw));
}
static void top_link(void){
    draw_header("CONNECTION DIAGNOSTICS");
    const char*association="DISCONNECTED";
    if(assoc_state==ASSOCSTATUS_SEARCHING)association="SEARCHING";
    if(assoc_state==ASSOCSTATUS_ASSOCIATING)association="ASSOCIATING";
    if(assoc_state==ASSOCSTATUS_ACQUIRINGDHCP)association="ACQUIRING IP";
    if(wifi_up)association="ASSOCIATED";
    char b[90];
    snprintf(b,sizeof(b),"WIFI: %s",association);text(top,10,32,b,wifi_up?GREEN:HIGHLIGHT,34);
    text(top,10,51,bridge_up?"BRICKOS HTTP: ONLINE":"BRICKOS HTTP: OFFLINE",bridge_up?GREEN:RED,34);
    text(top,10,70,ecu_up?(ecu_status_known?"ECU SESSION: CONNECTED":"ECU SESSION: LAST CONFIRMED"):
         ecu_status_known?"ECU SESSION: DISCONNECTED":"ECU SESSION: NOT CONFIRMED",
         ecu_up?GREEN:ecu_status_known?RED:HIGHLIGHT,34);
    text(top,10,89,ini_ok?"ECU INI: LOADED / PARSED":"ECU INI: NOT LOADED",ini_ok?GREEN:HIGHLIGHT,34);
    char ehash[11]="",ihash[11]="";
    bool eh=bt_signature_hash_copy(actual_sig,ehash);
    bool ih=bt_signature_hash_copy(def.signature,ihash);
    snprintf(b,sizeof(b),"ECU HASH: %s",eh?ehash:"NOT REPORTED");text(top,10,111,b,eh?WHITE:HIGHLIGHT,34);
    snprintf(b,sizeof(b),"INI HASH: %s",ih?ihash:def.signature[0]?"NO NUMERIC SUFFIX":"NO SIGNATURE");text(top,10,129,b,ih?WHITE:HIGHLIGHT,34);
    snprintf(b,sizeof(b),"%s MODE: %s",isDSiMode()?"DSI":"DS",sd_ready?"SD READY":sd_attempted?"SD FAILED / SEE BELOW":"A TO MOUNT SD");
    text(top,10,145,b,sd_ready?CYAN:sd_attempted?RED:HIGHLIGHT,34);
    if(bt_async_busy()){
        char state[48];snprintf(state,sizeof(state),"%s %lu B",bt_async_phase(),(unsigned long)bt_async_bytes());
        text(top,10,157,state,CYAN,34);
    }else if(frame<network_resume_frame){
        char state[48];snprintf(state,sizeof(state),"TCP BACKOFF %d S",(network_resume_frame-frame+59)/60);
        text(top,10,157,state,HIGHLIGHT,34);
    }
    snprintf(b,sizeof(b),"INI: %d CH  %d MENU  %d MAP",def.channel_count,def.menu_count,def.map_count);
    text(top,10,169,b,WHITE,35);
    if(last_response_line[0])snprintf(b,sizeof(b),"%.38s",last_response_line);
    else snprintf(b,sizeof(b),"HTTP %d   LIVE %dHZ ERR %d",last_http_status,measured_hz,live_errors);
    text(top,10,182,b,MUTED,34);
}
static void list_rows(int count,bool maps){
    if(count<1){text(bottom,10,65,maps?"NO COMPATIBLE MAPS YET":"WAITING FOR ECU DEFINITION",MUTED,34);return;}
    for(int row=0;row<8&&list_scroll+row<count;row++){
        int ix=list_scroll+row,y=28+row*16;
        bool selected=ix==list_pick;
        if(selected)round_fill(bottom,6,y,244,15,3,PANEL);
        if(maps){int id=map_from_row(ix);if(id>=0){
                text(bottom,12,y+3,def.maps[id].label,selected?HIGHLIGHT:WHITE,32);
                text(bottom,244,y+3,def.maps[id].curve?"C":"M",MUTED,1);
            }}
        else text(bottom,12,y+3,def.channels[ix].name,selected?HIGHLIGHT:WHITE,32);
    }
}
static void bottom_keypad(void){
    text(bottom,10,27,"ENTER ECU VALUE (STAGED ONLY)",HIGHLIGHT,29);
    round_fill(bottom,8,43,240,20,4,LITE);
    text(bottom,15,46,keypad_input[0]?keypad_input:"0",WHITE,23);
    for(int i=0;i<16;i++){
        int x=6+(i%4)*62, y=68+(i/4)*27;
        round_fill(bottom,x,y,57,23,4,i==14?LITE:PANEL);
        const char *key=keypad_keys[i];
        int tx=x+(57-(int)strlen(key)*9)/2;
        text(bottom,tx,y+5,key,i==14?GREEN:WHITE,6);
    }
    text(bottom,7,180,"A APPLY  /  B CANCEL",MUTED,29);
}
static void bottom_editor(void){
        const BtMap*map=selected_map();if(!map)return;
        char b[48];text(bottom,10,27,map->label,WHITE,29);
        snprintf(b,sizeof(b),"CELL %d,%d  OF %d X %d",cx+1,cy+1,table.width,table.height);
        text(bottom,10,46,b,MUTED,29);
        number(table.cells[cy*table.width+cx],b,sizeof(b));
        text(bottom,10,65,selected_map() && bt_field(&def,selected_map()->z,false) && bt_field(&def,selected_map()->z,false)->kind==2?"OPTION":"VALUE",MUTED,9);
        text(bottom,66,65,b,HIGHLIGHT,18);
        const BtField *edit_field=bt_field(&def,map->z,false);
        if(edit_field&&edit_field->kind==2){
            int option=(int)table.cells[cy*table.width+cx];
            if(option>=0&&option<bit_choice_count)
                text(bottom,10,104,bit_choice_labels[option],CYAN,25);
        }
        char step[24];number(steps[step_index],step,sizeof(step));
        snprintf(b,sizeof(b),"A: ENTER     STEP %s",step);
        text(bottom,10,87,b,WHITE,29);
        if(table.has_x){number(table.xbins[cx],b,sizeof(b));text(bottom,10,102,"X:",MUTED,2);text(bottom,31,102,b,WHITE,13);}
        if(table.has_y){number(table.ybins[cy],b,sizeof(b));text(bottom,135,102,"Y:",MUTED,2);text(bottom,156,102,b,WHITE,12);}
        static const char *actions[]={"-","+","STEP","TRACE"};
        for(int i=0;i<4;i++){
            int x=6+i*62;
            round_fill(bottom,x,120,57,26,4,i==1?LITE:PANEL);
            text(bottom,x+(57-(int)strlen(actions[i])*9)/2,127,actions[i],WHITE,5);
        }
        if(note[0])text(bottom,8,152,note,HIGHLIGHT,29);
        else text(bottom,8,152,table.dirty?"LOCAL EDIT / NOT WRITTEN":"ECU ORIGINAL / UNCHANGED",table.dirty?HIGHLIGHT:MUTED,29);
        text(bottom,8,171,table.confirm_burn?"R AGAIN: BACKUP + VERIFY":"R TWICE: VERIFIED BURN",HIGHLIGHT,30);
        return;
}
static void bottom_tune(void){
    /* The active editor takes priority over its parent nested dialog. */
    if(keypad_open){bottom_keypad();return;}
    if(chosen_map>=0&&table.valid){
        bottom_editor();
        return;
    }
    if(dialog_name[0]){
        text(bottom,10,30,dialog_label,HIGHLIGHT,34);
        int total=dialog_count();
        if(!total)text(bottom,10,74,"NO SUPPORTED FIELDS",MUTED,34);
        for(int row=0;row<6&&dialog_scroll+row<total;row++){
            int ix=dialog_scroll+row,at=dialog_at(ix),y=47+row*17;
            if(at<0)continue;
            bool sel=ix==dialog_pick;
            if(sel)round_fill(bottom,6,y,244,16,3,PANEL);
            text(bottom,12,y+4,def.refs[at].label,sel?HIGHLIGHT:WHITE,31);
        }
        text(bottom,9,150,"A: OPEN SETTING   B: BACK",MUTED,35);
        return;
    }
    int count=tune_item_count();
    const char*cat="ECU MENU";
    int index=category_at(tune_cat);if(index>=0)cat=def.menus[index].label;
    text(bottom,9,29,tune_group>=0?def.menus[tune_group].label:cat,HIGHLIGHT,34);
    if(!count){text(bottom,11,75,"NO ECU SETTINGS IN CATEGORY",MUTED,34);return;}
    for(int row=0;row<6&&list_scroll+row<count;row++){
        int idx=tune_item_at(list_scroll+row),y=47+row*17;
        if(idx<0)continue;
        bool selected=list_scroll+row==list_pick;
        if(selected)round_fill(bottom,6,y,244,16,3,PANEL);
        nav_icon_mini(bottom,category_icon(def.menus[idx].label),10,y+1);
        text(bottom,29,y+3,def.menus[idx].label,selected?HIGHLIGHT:WHITE,26);
        if(def.menus[idx].group==1)text(bottom,240,y+4,">",CYAN,1);
        else if(map_for_target(def.menus[idx].target)>=0)text(bottom,240,y+4,"M",CYAN,1);
    }
    text(bottom,9,150,tune_focus?"A: OPEN   B: CATEGORIES":"UP/DOWN: CATEGORY   A: MENU",MUTED,35);
}
/* 3DS layout parity: workspace launcher is a vertical icon-left list.
 * The upper screen remains exclusively the live dashboard. */
static void bottom_home(void){
    text(bottom,8,5,"BRICK TUNER / WORKSPACES",HIGHLIGHT,26);
    for(int i=0;i<6;i++){
        int y=27+i*24;
        bool selected=home_pick==i;
        round_fill(bottom,6,y,244,22,4,selected?LITE:PANEL);
        if(selected)fill(bottom,7,y+4,3,14,HIGHLIGHT);
        nav_icon_mini(bottom,home_icons[i],14,y+4);
        text(bottom,34,y+4,home_labels[i],selected?WHITE:MUTED,23);
        text(bottom,237,y+4,">",selected?HIGHLIGHT:EDGE,1);
    }
    text(bottom,9,175,"UP/DOWN  A OPEN  L/R GAUGES",HIGHLIGHT,27);
}
static void bottom_idle_gauges(void){
    text(bottom,9,6,"LIVE ECU / IDLE INSTRUMENTS",HIGHLIGHT,29);
    if(idle_gauges.count==0){
        text(bottom,11,85,"NO ADDITIONAL ECU CHANNELS",MUTED,29);
        text(bottom,11,105,"ANY CONTROL: RETURN TO MENU",WHITE,29);
        return;
    }
    int hidden=gauge_layout>2?gauge_layout-2:0;
    for(int i=hidden;i<idle_gauges.count;i++){
        int display=i-hidden;
        int idx=idle_gauges.field_index[i];
        if(idx<0||idx>=def.channel_count)continue;
        const BtField *f=&def.channels[idx];
        int x=6+(display%2)*124, y=27+(display/2)*49;
        round_fill(bottom,x,y,119,45,5,PANEL);
        if(i==0)fill(bottom,x+3,y+6,2,29,HIGHLIGHT);
        /* Keep text legible at 256x192: labels on one line, values at 2x. */
        char label[16];snprintf(label,sizeof(label),"%.14s",f->name);
        text(bottom,x+7,y+3,label,MUTED,14);
        double value=0;bool ok=get_value(f->name,&value);
        char v[24];if(ok)number(value,v,sizeof(v));else snprintf(v,sizeof(v),"--");
        if(strlen(v)>7)v[7]=0;
        /* Large 2x text at 16-pixel pitch, up to six chars. */
        for(int j=0;v[j]&&j<6;j++)glyph(bottom,x+7+j*16,y+19,(unsigned char)v[j],ok?WHITE:MUTED,2);
    }
    text(bottom,8,176,"ANY KEY OR TOUCH: RETURN",HIGHLIGHT,29);
}
static void top_sounds(void){
    draw_header("EVENT AUDIO");
    text(top,12,40,"ORIGINAL 3DS SOUND EFFECTS",HIGHLIGHT,30);
    text(top,12,62,"14 ORIGINAL 3DS PCM CUES",WHITE,28);
    text(top,12,90,"SELECT A CLIP BELOW",MUTED,30);
    text(top,12,114,"A: PLAY SELECTED SOUND",WHITE,30);
    text(top,12,140,sd_ready?bts_sfx_status(sd_ready):"MOUNT SD IN LINK",
         bts_sfx_files_ready(sd_ready)?GREEN:HIGHLIGHT,26);
    text(top,12,158,"COPY ALL SFX FILES TO SD",MUTED,25);
}
static void bottom_sounds(void){
    int first=sound_pick>5?sound_pick-5:0;
    for(int i=first;i<BTS_SOUND_COUNT&&i<first+7;i++){
        int y=28+(i-first)*20;
        if(i==sound_pick)round_fill(bottom,7,y,242,19,3,PANEL);
        text(bottom,13,y+4,sound_labels[i],i==sound_pick?HIGHLIGHT:WHITE,27);
    }
    text(bottom,9,164,bts_sfx_status(sd_ready),bts_sfx_files_ready(sd_ready)?GREEN:HIGHLIGHT,27);
    text(bottom,9,180,"A: PLAY    B: HOME",MUTED,27);
}
static void bottom_graph(void){
    text(bottom,10,30,"GRAPH CHANNEL",HIGHLIGHT,34);
    for(int i=0;i<SLOTS;i++){
        int x=7+i*124,y=45;
        bool selected=i==active_slot;
        round_fill(bottom,x,y,116,35,4,selected?LITE:PANEL);
        text(bottom,x+6,y+13,channel_names[i],selected?HIGHLIGHT:WHITE,15);
    }
    text(bottom,10,150,"Y: GRAPH / MIN MAX AVG",MUTED,34);
}
static void bottom_link(void){
    text(bottom,10,30,"EXPECTED NETWORK: " EXPECTED_AP,HIGHLIGHT,34);
    text(bottom,10,47,"OPEN NETWORK / DS SLOT 1-3",MUTED,34);
    text(bottom,10,65,sd_ready?(cache_loaded?"SD READY / INI CACHED":"SD READY / INI TO SD"):sd_attempted?bt_sd_message():"A: MOUNT SD AND ENABLE INI",sd_ready?GREEN:sd_attempted?RED:HIGHLIGHT,34);
    text(bottom,10,89,"X: RECONNECT SAVED WI-FI",WHITE,34);
    text(bottom,10,106,"Y: RETEST BRICKOS / ECU",WHITE,34);
    /* A live GET does not change the established link state to WAITING. */
    text(bottom,10,123,(bt_async_busy()&&bt_async_job()==BT_NET_INI)?
         bt_async_phase():status_msg,HIGHLIGHT,34);
    if(loading_cache){
        char buf[48];snprintf(buf,sizeof(buf),"CACHED INI PARSE %lu B",(unsigned long)bt_cache_bytes());
        text(bottom,10,140,buf,WHITE,34);
    }else if(loading_ini || (bt_async_busy()&&bt_async_job()==BT_NET_INI)){
        char buf[48];
        snprintf(buf,sizeof(buf),"RX %lu   SD %lu",(unsigned long)ini_progress,(unsigned long)ini_disk_bytes);
        text(bottom,10,138,buf,WHITE,34);
        if(ini_expected_bytes)
            snprintf(buf,sizeof(buf),"PARSE %lu / %lu B",(unsigned long)ini_parsed_bytes,(unsigned long)ini_expected_bytes);
        else snprintf(buf,sizeof(buf),"PARSE %lu  / STREAM",(unsigned long)ini_parsed_bytes);
        text(bottom,10,151,buf,MUTED,34);
    }else if(last_net_error[0]){
        text(bottom,10,137,last_net_error,RED,34);
        if(last_reject_body[0])text(bottom,10,150,last_reject_body,HIGHLIGHT,34);
        else if(last_failed_path[0])text(bottom,10,150,last_failed_path,HIGHLIGHT,34);
    }
    else if(def.signature[0]&&!bt_signature_hash_copy(def.signature,(char[11]){0})){
        char b[64];snprintf(b,sizeof(b),"INI SIG: %.34s",def.signature);
        text(bottom,10,140,b,MUTED,34);
    }else text(bottom,10,140,"IP / DNS ARE LOCAL ONLY",MUTED,34);
}
static void render(void){
    /* Never take over the UI while downloading. Touchscreen and navigation
     * remain accessible throughout the status/INI/live network phases. */
    if(page==P_HOME)top_dashboard();
    else if(page==P_TUNE)top_tune();
    else if(page==P_MAPS)map_top();
    else if(page==P_LIVE)top_channels();
    else if(page==P_GRAPH)top_graph();
    else if(page==P_SOUNDS)top_sounds();
    else top_link();
    fill(bottom,0,0,W,H,BG);fill(bottom,0,0,W,23,PANEL);fill(bottom,0,22,W,1,EDGE);
    if(page!=P_HOME){text(bottom,9,6,"< HOME",HIGHLIGHT,6);text(bottom,94,6,tabs[page],WHITE,18);}
    if(idle_dashboard)bottom_idle_gauges();
    else if(page==P_HOME)bottom_home();
    else if(page==P_TUNE)bottom_tune();
    else if(page==P_MAPS){if(chosen_map>=0&&table.valid)bottom_tune();else list_rows(map_count,true);}
    else if(page==P_LIVE)list_rows(def.channel_count,false);
    else if(page==P_GRAPH)bottom_graph();
    else if(page==P_SOUNDS)bottom_sounds();
    else bottom_link();
    if((loading_ini||loading_cache) && page!=P_LINK){
        char b[42];size_t bytes=loading_cache?bt_cache_bytes():bt_async_bytes();
        if(loading_cache)snprintf(b,sizeof(b),"CACHE PARSE: %lu B",(unsigned long)bytes);
        else if(bt_async_job()==BT_NET_INI && bt_async_busy() && bt_async_parsed_bytes())
            snprintf(b,sizeof(b),"INI PARSE: %lu B",(unsigned long)bt_async_parsed_bytes());
        else if(bt_async_job()==BT_NET_INI && bt_async_busy() && bt_async_saved_bytes())
            snprintf(b,sizeof(b),"INI RX %lu / SD %lu",(unsigned long)bt_async_bytes(),(unsigned long)bt_async_saved_bytes());
        else snprintf(b,sizeof(b),"INI RECEIVE: %lu B",(unsigned long)bytes);
        fill(bottom,3,151,250,14,PANEL);
        text(bottom,9,153,b,HIGHLIGHT,34);
    }
    /* Drawing always targets the off-screen RAM framebuffer.
     * Present once at VBlank from the main loop to avoid double waits. */
}
static void present(void){
    memcpy(lcd_top,top,W*H*sizeof(uint16_t));
    memcpy(lcd_bottom,bottom,W*H*sizeof(uint16_t));
}
static void sample_hist(void){
    for(int i=0;i<SLOTS;i++){
        double v=0;bool valid=get_value(channel_names[i],&v);
        hist_valid[i][history_at]=valid;if(valid){
            hist[i][history_at]=v;
            if(!sample_count[i]||v<low[i])low[i]=v;
            if(!sample_count[i]||v>high[i])high[i]=v;
            sum[i]+=v;sample_count[i]++;
        }
    }
    history_at=(history_at+1)%HISTORY;
}
static void update_wifi(void){
    assoc_state=wifi_initialized?Wifi_AssocStatus():ASSOCSTATUS_DISCONNECTED;
    bool connected=assoc_state==ASSOCSTATUS_ASSOCIATED;
    if(connected&&!wifi_up){
        wifi_up=true;current_ip();want_status=true;
        snprintf(status_msg,sizeof(status_msg),"WIFI ASSOCIATED / TESTING ESP32");
    }else if(!connected&&wifi_up){
        wifi_up=false;bridge_up=false;ecu_up=false;ecu_status_known=false;want_ini=false;want_status=false;
        network_failure_count=0;network_resume_frame=0;last_socket_begin_frame=-1000;
        live_failure_streak=0;live_resume_frame=0;
        loading_ini=false;bt_async_cancel();bt_async_forget_connection();set_compat();invalidate();
        snprintf(client_ip,sizeof(client_ip),"-");snprintf(gateway_ip,sizeof(gateway_ip),"-");
        snprintf(status_msg,sizeof(status_msg),"WIFI CONNECTION LOST");
    }
}
/* Status processing runs only after a *complete* nonblocking HTTP response. */
static void check_status(const char*b){
    bool connected=false;
    bridge_up=true;
    if(!bt_json_bool(b,"ecu_session_connected",&connected)){
        ecu_status_known=false;set_compat();snprintf(status_msg,sizeof(status_msg),"BRICKOS STATUS FIELD MISSING");return;
    }
    bool previous=ecu_up;ecu_up=connected;ecu_status_known=true;
    if(!previous&&connected)bts_sfx_play(BTS_SOUND_CONNECTED,sd_ready);
    if(ecu_up){
        char sig[152]="";bt_json_string(b,"ecu_signature",sig,sizeof(sig));
        if(sig[0]&&strcmp(sig,actual_sig)){
            /* Prefix/name changes are NOT a new ECU layout. Only invalidate
             * loaded field offsets if two real layout hashes disagree. */
            char old_hash[11],new_hash[11];
            bool changed=bt_signature_hash_copy(actual_sig,old_hash)&&
                bt_signature_hash_copy(sig,new_hash)&&strcmp(old_hash,new_hash)!=0;
            snprintf(actual_sig,sizeof(actual_sig),"%s",sig);
            if(changed){
                bt_ini_reset(&def);build_map_index();invalidate();
                cache_loaded=false;
                chosen_map=-1;table.valid=false;list_pick=list_scroll=0;
                tune_cat=tune_focus=0;tune_group=-1;dialog_name[0]=0;
            }
            if(changed||!def.valid)want_ini=true;
            ini_retry_frame=0;
        }
        if(!previous&&!def.valid)want_ini=true;
        bt_json_string(b,"brick_os_build",firmware,sizeof(firmware));
        snprintf(status_msg,sizeof(status_msg),"BRICKOS OK / ECU USB CONNECTED");
    }else{
        if(previous)invalidate();
        snprintf(status_msg,sizeof(status_msg),"BRICKOS ONLINE / ECU USB OFFLINE");
    }
    set_compat();
    if(ini_ok){want_ini=false;snprintf(status_msg,sizeof(status_msg),"ECU INI LOADED / LIVE READY");}
}
static void network_finished(BtNetJob job,int result){
    if(result<0){
        last_http_status=bt_async_http_status();
        last_failed_job=job;
        snprintf(last_net_error,sizeof(last_net_error),"%.78s",bt_async_error());
        snprintf(last_failed_path,sizeof(last_failed_path),"GET %s",bt_async_request_path());
        snprintf(last_response_line,sizeof(last_response_line),"%s",bt_async_status_line());
        snprintf(last_reject_body,sizeof(last_reject_body),"%s",bt_async_reject_body());
        if(last_http_status!=200 && last_http_status>0){
            /* Keep the raw HTTP status and ESP error body for inspection. */
            bt_sd_http_report(bt_async_request_path(),bt_async_status_line(),
                              last_http_status,bt_async_reject_body());
        }
        if(last_http_status!=200 && last_http_status>0 && last_response_line[0])
            snprintf(status_msg,sizeof(status_msg),"%.90s",last_response_line);
        if(job!=BT_NET_LIVE)network_failure();
        if(job==BT_NET_STATUS){
            /* A failed HTTP call cannot establish whether the ECU is online. */
            /* Previous confirmed ECU state remains valid until an explicit
             * status or real Wi-Fi loss says otherwise. Retry status quietly. */
            want_status=true;
            set_compat();
            snprintf(status_msg,sizeof(status_msg),"HTTP STATUS RETRY / ECU LAST KNOWN");
        }else if(job==BT_NET_INI){
            /* If Wi-Fi or SD loses a transfer, recover the last validated
             * INI from SD. A partial download must not erase working menus. */
            /* Do not wipe the already-loaded SD metadata if downloading
             * an optional refresh fails. */
            ini_hash_mismatch=false;
            ini_retry_frame=frame+600;
            loading_cache=(!def.valid)&&bt_sd_ready()&&bt_cache_begin(&def);
            snprintf(status_msg,sizeof(status_msg),loading_cache?
                "INI FAILED / RESTORING SD CACHE":"INI TRANSFER FAILED / RETRY");
        }else if(job==BT_NET_LIVE){
            live_errors++;
            if(last_http_status==503){
                /* BrickOS /api/live subscribes BEFORE returning HTTP 503 when
                 * its worker has not yet produced a fresh sample. Interest
                 * expires after 4s; 2..16s client backoff made it permanent. */
                live_failure_streak=0;
                live_resume_frame=frame+6; /* 100ms warm-up; below 4s lease */
            }else{
                if(live_failure_streak<5)live_failure_streak++;
                int pause=120 << (live_failure_streak-1);
                if(pause>960)pause=960;
                live_resume_frame=frame+pause;
            }
            /* Keep the prior status, parsed INI and last real gauges.
             * Next regular status poll independently checks ECU health. */
        }
        return;
    }
    network_success(job);
    bridge_up=true;
    if(job==BT_NET_INI||job==BT_NET_STATUS||job==BT_NET_LIVE)last_http_status=bt_async_http_status();
    if(job==BT_NET_STATUS){
        check_status(bt_async_status_json());
    }else if(job==BT_NET_INI){
        ini_hash_mismatch=false;
        cache_loaded=true;
        build_map_index();set_compat();ini_retry_frame=0;
        if(ini_ok) (void)bt_sd_snapshot_save(&def);
        snprintf(status_msg,sizeof(status_msg),"%s",ini_ok?"ECU INI LOADED / SD CACHED":"INI PARSE FAILED");
        list_pick=list_scroll=0;
        if(!ini_ok)ini_retry_frame=frame+600;
    }else if(job==BT_NET_LIVE){
        live_frame=frame;
        if(bt_async_live_is_new()){
            live_count++;sample_hist();
        }
    }
}
static int live_length(void){
    const char*all[SLOTS+3];
    for(int i=0;i<SLOTS;i++)all[i]=channel_names[i];
    all[SLOTS]=page==P_LIVE&&list_pick<def.channel_count?def.channels[list_pick].name:channel_names[0];
    const BtMap*m=(chosen_map>=0&&table.valid)?selected_map():NULL;
    all[SLOTS+1]=m&&m->xchan[0]?m->xchan:channel_names[0];
    all[SLOTS+2]=m&&m->ychan[0]?m->ychan:channel_names[0];
    int length=bt_required_live_length(&def,all,SLOTS+3);
    /* Preserve a small 30Hz response while navigating; request the larger
     * six-channel capture only when the lower idle dashboard is visible. */
    if(idle_dashboard || (page==P_HOME&&gauge_layout>2))length=bt_idle_required_length(&def,&idle_gauges,length);
    return length;
}
static void service_network(void){
    if(loading_cache){
        /* Cache parsing is a one-time fallback, never the steady-state
         * launch path. Process a few KB per frame without blocking touch. */
        int result=0;
        for(int n=0;n<4 && result==0;n++)result=bt_cache_poll();
        ini_parsed_bytes=bt_cache_bytes();
        if(result){
            loading_cache=false;
            cache_loaded=result>0;
            if(cache_loaded){
                build_map_index();set_compat();
                (void)bt_sd_snapshot_save(&def);
                snprintf(status_msg,sizeof(status_msg),"SD INI READY / VERIFY ECU");
            }else snprintf(status_msg,sizeof(status_msg),bt_cache_out_of_memory()?"INI CACHE OUT OF RAM":"INVALID SD INI / WILL RE-DOWNLOAD");
            if(wifi_up)want_status=true;
        }
        return;
    }
    if(!wifi_up){if(bt_async_busy())bt_async_cancel();return;}
    if(bt_async_busy()){
        BtNetJob job=bt_async_job();
        int result=0;
        /* Multiple bounded nonblocking steps can complete one HTTP request
         * in two VBlanks without waiting for a whole extra video frame for
         * each socket send, header and body. INI SD writes stay slower. */
        int budget=job==BT_NET_INI?1:4;
        for(int n=0;n<budget && result==0;n++)result=bt_async_poll(frame);
        if(job==BT_NET_INI){
            ini_progress=bt_async_bytes();
            ini_disk_bytes=bt_async_saved_bytes();
            ini_parsed_bytes=bt_async_parsed_bytes();
            ini_expected_bytes=bt_async_total();
        }
        if(result){
            loading_ini=false;
            network_finished(job,result);
            bt_async_cancel();
        }
        return;
    }
    if(ecu_up&&ini_retry_frame>0&&frame>=ini_retry_frame){
        ini_retry_frame=0;want_ini=true;
    }
    /* Never spin on E12/ENOMEM, and never flood the tiny DSWiFi socket pool. */
    if(frame<network_resume_frame||frame-last_socket_begin_frame<BTS_SOCKET_GAP_FRAMES)return;
    /* One in-flight request at a time; INI is never interrupted by live polling. */
    if(want_status||frame-network_poll>=BTS_STATUS_POLL_FRAMES){
        want_status=false;network_poll=frame;
        if(!bt_async_begin(BT_NET_STATUS,NULL,NULL,0,frame)){
            snprintf(last_net_error,sizeof(last_net_error),"%.78s",bt_async_error());
            bridge_up=false;ecu_status_known=false;want_status=true;
            network_failure();set_compat();bt_async_cancel();
        }else last_socket_begin_frame=frame;
    }else if(want_ini&&sd_ready&&ecu_up&&actual_sig[0]){
        want_ini=false;loading_ini=true;ini_progress=ini_disk_bytes=ini_parsed_bytes=ini_expected_bytes=0;
        snprintf(status_msg,sizeof(status_msg),"DOWNLOADING ECU INI");
        bt_async_expected_signature(actual_sig);
        if(!sd_ready || !bt_async_begin(BT_NET_INI,&def,NULL,0,frame)){
            snprintf(last_net_error,sizeof(last_net_error),"%.78s",sd_ready?bt_async_error():bt_sd_message());
            loading_ini=false;ini_retry_frame=frame+600;
            network_failure();bt_async_cancel();
        }else last_socket_begin_frame=frame;
        /* The downloader clears metadata before re-parsing the new file;
         * immediately withdraw stale UI/live access while it is in flight. */
        set_compat();
    }else if(ini_ok&&ecu_up&&bridge_up&&frame>=live_resume_frame&&
             frame-live_poll>=BTS_LIVE_POLL_FRAMES){
        live_poll=frame;
        if(!bt_async_begin(BT_NET_LIVE,NULL,&live,live_length(),frame)){
            snprintf(last_net_error,sizeof(last_net_error),"%.78s",bt_async_error());
            live_errors++;
            if(live_failure_streak<5)live_failure_streak++;
            int pause=120 << (live_failure_streak-1);
            if(pause>960)pause=960;
            live_resume_frame=frame+pause;
            bt_async_cancel();
        }else last_socket_begin_frame=frame;
    }
}
static void open_map(int id){
    if(id<0||(id>=def.map_count&&id!=BTS_MAX_MAPS)||!ini_ok)return;
    const BtMap*m=id==BTS_MAX_MAPS?&scalar_map:&def.maps[id];
    const BtField*z=bt_field(&def,m->z,false);
    if(!z||!z->scaled||z->width*z->height>BTS_MAP_CELLS){
        snprintf(note,sizeof(note),"MAP TYPE UNSUPPORTED");return;
    }
    memset(&table,0,sizeof(table));
    table.width=z->width;table.height=z->height;
    table.raw_length=bt_type_size(z->type)*z->width*z->height;
    /* Don't overlap the intentional synchronous ECU read with a live poll. */
    bt_async_cancel();bt_async_forget_connection();
    snprintf(status_msg,sizeof(status_msg),"READING ORIGINAL ECU MAP");render();swiWaitForVBlank();present();
    if(table.raw_length<1||table.raw_length>(int)sizeof(table.baseline)||
       !bt_read_raw(&def,z,table.baseline,sizeof(table.baseline))){
        snprintf(status_msg,sizeof(status_msg),"ECU MAP READ FAILED");return;
    }
    memcpy(table.edited,table.baseline,(size_t)table.raw_length);
    BtField rel=*z;rel.offset=0;
    for(int i=0;i<z->width*z->height;i++){
        if(!bt_field_value(&def,&rel,table.baseline,table.raw_length,i,&table.cells[i])){
            snprintf(status_msg,sizeof(status_msg),"INI FIELD SCALING UNSUPPORTED");return;
        }
    }
    const BtField*x=bt_field(&def,m->x,false),*y=bt_field(&def,m->y,false);
    if(x&&x->scaled&&x->width*x->height>=z->width&&x->width*x->height<=64){
        table.has_x=bt_read_field(&def,x,table.xbins,64);
        if(table.has_x)table.x_count=x->width*x->height;
    }
    if(y&&y->scaled&&y->width*y->height>=z->height&&y->width*y->height<=64){
        table.has_y=bt_read_field(&def,y,table.ybins,64);
        if(table.has_y)table.y_count=y->width*y->height;
    }
    chosen_map=id;cx=cy=0;table.valid=true;abandon_confirm=false;
}
/* All keypad and +/- edits pass through the same INI-scaled encoder.
 * This only changes the local staged buffer, never ECU memory. */
static bool set_map_cell(double value){
    if(chosen_map<0||!table.valid||!ini_ok||!isfinite(value))return false;
    const BtMap *m=selected_map();if(!m)return false;
    const BtField *z=bt_field(&def,m->z,false);
    if(!z||!z->scaled){snprintf(note,sizeof(note),"INI SCALING NOT SUPPORTED");return false;}
    if(z->kind==2&&bit_choice_count>0){
        int choice=(int)value;
        if(value!=floor(value)||choice<0||choice>=bit_choice_count||!bit_choice_labels[choice][0]){
            snprintf(note,sizeof(note),"CHOICE NOT DEFINED BY ECU INI");return false;
        }
    }
    int cell=cy*table.width+cx,size=bt_type_size(z->type);
    if(size<1||size>8||cell<0||cell>=BTS_MAP_CELLS||cell*size+size>table.raw_length)return false;
    BtField rel=*z;rel.offset=0;
    uint8_t previous[8];memcpy(previous,table.edited+cell*size,(size_t)size);
    if(!bt_encode_value(&def,&rel,table.edited,table.raw_length,cell,value)||
       !bt_field_value(&def,&rel,table.edited,table.raw_length,cell,&value)){
        memcpy(table.edited+cell*size,previous,(size_t)size);
        snprintf(note,sizeof(note),"VALUE OUTSIDE ECU INI LIMITS");return false;
    }
    table.cells[cell]=value;
    table.dirty=memcmp(table.edited,table.baseline,(size_t)table.raw_length)!=0;
    table.confirm_burn=false;abandon_confirm=false;note[0]=0;
    return true;
}
static void change_map_cell(int direction){
    if(chosen_map<0||!table.valid)return;
    const BtMap *m=selected_map();if(!m)return;
    const BtField *z=bt_field(&def,m->z,false);if(!z)return;
    double delta=z->kind==2?1.0:steps[step_index];
    if(fabs(z->scale)>delta)delta=fabs(z->scale);
    double next=table.cells[cy*table.width+cx]+delta*direction;
    if(z->kind==2&&bit_choice_count>0){
        for(int attempt=0;attempt<bit_choice_count;attempt++){
            int index=(int)next;
            if(index>=0&&index<bit_choice_count&&bit_choice_labels[index][0])break;
            next+=direction;
        }
    }
    (void)set_map_cell(next);
}
static void keypad_begin(void){
    if(!table.valid||chosen_map<0)return;
    snprintf(keypad_input,sizeof(keypad_input),"%.8g",table.cells[cy*table.width+cx]);
    keypad_open=true;
}
static void keypad_apply(void){
    char *end=NULL;
    double value=strtod(keypad_input,&end);
    if(!keypad_input[0]||!end||*end||!isfinite(value)){
        snprintf(note,sizeof(note),"ENTER A FINITE NUMBER");return;
    }
    if(set_map_cell(value))keypad_open=false;
}
static void keypad_key(int key){
    if(key<0||key>=16)return;
    if(key==14){keypad_apply();return;}
    if(key==15){keypad_open=false;return;}
    size_t n=strlen(keypad_input);
    if(key==3){if(n)keypad_input[n-1]=0;return;}
    if(key==7){keypad_input[0]=0;return;}
    if(key==11){
        if(keypad_input[0]=='-')memmove(keypad_input,keypad_input+1,n);
        else if(n+1<sizeof(keypad_input)){memmove(keypad_input+1,keypad_input,n+1);keypad_input[0]='-';}
        return;
    }
    const char *k=keypad_keys[key];
    if(key==12&&strchr(keypad_input,'.'))return;
    if(n+1<sizeof(keypad_input)){
        keypad_input[n]=k[0];keypad_input[n+1]=0;
    }
}
static void commit_map(void){
    if(!table.dirty||!selected_map()||!ini_ok){snprintf(note,sizeof(note),"NO PENDING MAP EDITS");return;}
    if(!table.confirm_burn){table.confirm_burn=true;
        snprintf(note,sizeof(note),"R AGAIN: BENCH ONLY / BACKUP / BURN");return;}
    table.confirm_burn=false;
    bt_async_cancel();bt_async_forget_connection(); /* Write path owns transport. */
    const BtMap*m=selected_map();if(!m)return;
    const BtField*z=bt_field(&def,m->z,false);
    if(!z){snprintf(note,sizeof(note),"INI FIELD LOST / NO WRITE");return;}
    snprintf(status_msg,sizeof(status_msg),"VERIFYING ECU / WRITING BENCH ONLY");render();swiWaitForVBlank();present();
    if(bt_commit_field(&def,z,table.baseline,table.edited,table.raw_length,note,sizeof(note))){
        memcpy(table.baseline,table.edited,(size_t)table.raw_length);
        table.dirty=false;snprintf(status_msg,sizeof(status_msg),"ECU BURN VERIFIED");
    }else snprintf(status_msg,sizeof(status_msg),"WRITE REFUSED / SEE LOWER SCREEN");
}
static void goto_page(int next){
    if(next<0||next>=P_COUNT)return;
    page=next;chosen_map=-1;table.valid=false;bit_choice_count=0;list_pick=list_scroll=0;
    keypad_open=false;
    activity_frame=frame;idle_dashboard=false;
    dialog_name[0]=0;dialog_value_ok=false;note[0]=0;dialog_depth=0;
    tune_focus=0;tune_group=-1;
}
static void navigate(int direction){
    int count=page==P_LIVE?def.channel_count:page==P_MAPS?map_count:tune_item_count();
    if(count<=0)return;
    list_pick+=direction;if(list_pick<0)list_pick=0;if(list_pick>=count)list_pick=count-1;
    if(list_pick<list_scroll)list_scroll=list_pick;
    int visible=page==P_TUNE?6:8;
    if(list_pick>=list_scroll+visible)list_scroll=list_pick-visible+1;
}
static void open_tune_item(void){
    int id=tune_item_at(list_pick);if(id<0)return;
    const BtMenu*m=&def.menus[id];
    bts_sfx_play(bts_sound_for_entry(m->label,m->target),sd_ready);
    if(m->group==1){tune_group=id;list_pick=list_scroll=0;return;}
    int map=map_for_target(m->target);
    if(map>=0){open_map(map);return;}
    for(int i=0;i<def.ref_count;i++)if(!strcasecmp(m->target,def.refs[i].owner)){
        open_dialog(m->target,m->label);note[0]=0;return;
    }
    /* Some valid INI menus link directly to a scalar or array constant,
     * without an intermediate dialog. Do not discard these tunable fields. */
    const BtField *field=bt_field(&def,m->target,false);
    if(field&&field->scaled&&field->width*field->height>0&&
       field->width*field->height<=BTS_MAP_CELLS){
        bit_choice_count=0;
        if(field->kind==2&&sd_ready)
            bit_choice_count=bt_bit_choices(bt_sd_cache_path(),field->name,bit_choice_labels,32);
        memset(&scalar_map,0,sizeof(scalar_map));
        snprintf(scalar_map.label,sizeof(scalar_map.label),"%s",m->label);
        snprintf(scalar_map.z,sizeof(scalar_map.z),"%s",field->name);
        open_map(BTS_MAX_MAPS);
        return;
    }
    snprintf(note,sizeof(note),"%.32s / NO EDITABLE FIELD",m->label);
}
static void handle_buttons(uint32_t down,uint32_t held,touchPosition*tp){
    if(keypad_open){
        if(down&KEY_B){keypad_open=false;return;}
        if(down&KEY_A){keypad_apply();return;}
        if((down&KEY_TOUCH)&&tp->py>=68&&tp->py<176){
            int col=(tp->px-6)/62,row=(tp->py-68)/27;
            if(tp->px>=6&&col>=0&&col<4&&row>=0&&row<4){
                keypad_key(row*4+col);
            }
        }
        return;
    }
    if(idle_dashboard && (down||held)){
        /* The first key/touch wakes the menu. It must not open an item or
         * accidentally change an ECU value under an invisible control. */
        activity_frame=frame;idle_dashboard=false;
        return;
    }
    if(chosen_map>=0&&table.valid){
        if((down|held)&KEY_LEFT&&cx>0)cx--;
        if((down|held)&KEY_RIGHT&&cx+1<table.width)cx++;
        if((down|held)&KEY_UP&&cy>0)cy--;
        if((down|held)&KEY_DOWN&&cy+1<table.height)cy++;
        if(down&KEY_X)change_map_cell(1);
        if(down&KEY_Y)change_map_cell(-1);
        if(down&KEY_A)keypad_begin();
        if(down&KEY_L){step_index=(step_index+1)%4;table.confirm_burn=false;}
        if((down&KEY_TOUCH)&&tp->py>=120&&tp->py<146&&tp->px>=6&&tp->px<254){
            int action=(tp->px-6)/62;
            if(action==0)change_map_cell(-1);
            else if(action==1)change_map_cell(1);
            else if(action==2){step_index=(step_index+1)%4;table.confirm_burn=false;}
            else if(action==3){
                map_live_trace(selected_map());
                if(trace_valid){cx=trace_x;cy=trace_y;table.confirm_burn=false;}
            }
        }
        if(down&KEY_R)commit_map();
        if(down&KEY_B){
            if(table.dirty&&!abandon_confirm){abandon_confirm=true;table.confirm_burn=false;
                snprintf(note,sizeof(note),"B AGAIN TO DISCARD UNSAVED EDITS");return;}
            chosen_map=-1;table.valid=false;note[0]=0;abandon_confirm=false;
        }
        return;
    }
    if((down&KEY_SELECT)&&page!=P_HOME){goto_page(P_HOME);return;}
    if((down&KEY_TOUCH)&&page!=P_HOME&&tp->py<23){goto_page(P_HOME);return;}
    if(page==P_HOME){
        if((down&KEY_UP)&&home_pick>0)home_pick--;
        if((down&KEY_DOWN)&&home_pick<5)home_pick++;
        if(down&KEY_L){gauge_layout=gauge_layout==2?4:gauge_layout-1;if(sd_ready)bt_sd_layout_save(gauge_layout);}
        if(down&KEY_R){gauge_layout=gauge_layout==4?2:gauge_layout+1;if(sd_ready)bt_sd_layout_save(gauge_layout);}
        if(down&KEY_A)goto_page(home_pages[home_pick]);
        if((down&KEY_TOUCH)&&tp->py>=27&&tp->py<171){
            int row=(tp->py-27)/24;
            if(row>=0&&row<6){home_pick=row;goto_page(home_pages[home_pick]);}
        }
    }else if(page==P_TUNE){
        if(dialog_name[0]){
            int n=dialog_count();
            if(down&KEY_B){
                if(dialog_depth>0){
                    dialog_depth--;
                    memcpy(dialog_name,dialog_owner_stack[dialog_depth],sizeof(dialog_name));
                    memcpy(dialog_label,dialog_label_stack[dialog_depth],sizeof(dialog_label));
                    dialog_pick=dialog_scroll=0;
                }else dialog_name[0]=0;
                dialog_value_ok=false;return;
            }
            if((down&KEY_UP)&&dialog_pick>0)dialog_pick--;
            if((down&KEY_DOWN)&&dialog_pick+1<n)dialog_pick++;
            if(dialog_pick<dialog_scroll)dialog_scroll=dialog_pick;
            if(dialog_pick>=dialog_scroll+6)dialog_scroll=dialog_pick-5;
            if((down&KEY_TOUCH)&&tp->py>=47&&tp->py<149){
                dialog_pick=dialog_scroll+(tp->py-47)/17;
                if(dialog_pick>=n)dialog_pick=n-1;
                if(dialog_pick>=0)dialog_action();
            }
            if(down&KEY_A)dialog_action();
            return;
        }
        if(down&KEY_B){
            if(tune_group>=0){tune_group=-1;list_pick=list_scroll=0;}
            else if(tune_focus){tune_focus=0;list_pick=list_scroll=0;}
            else goto_page(P_HOME);
        }
        if(tune_focus){
            if(down&KEY_UP)navigate(-1);
            if(down&KEY_DOWN)navigate(1);
            if((down&KEY_TOUCH)&&tp->py>=47&&tp->py<149){
                list_pick=list_scroll+(tp->py-47)/17;
                if(list_pick>=tune_item_count())list_pick=tune_item_count()-1;
                if(list_pick>=0)open_tune_item();
            }
            if(down&KEY_A)open_tune_item();
        }else{
            int total=category_count();
            if((down&KEY_UP)&&tune_cat>0){tune_cat--;list_pick=list_scroll=0;}
            if((down&KEY_DOWN)&&tune_cat+1<total){tune_cat++;list_pick=list_scroll=0;}
            if(down&KEY_A)tune_focus=1;
            if((down&KEY_TOUCH)&&tp->py>=47&&tp->py<149){
                tune_focus=1;list_pick=list_scroll+(tp->py-47)/17;
                if(list_pick>=tune_item_count())list_pick=tune_item_count()-1;
                if(list_pick>=0)open_tune_item();
            }
        }
    }else if(page==P_MAPS||page==P_LIVE){
        if(down&KEY_B)goto_page(P_HOME);
        if(down&KEY_UP)navigate(-1);
        if(down&KEY_DOWN)navigate(1);
        if((down&KEY_TOUCH)&&tp->py>=28&&tp->py<156){
            list_pick=list_scroll+(tp->py-28)/16;
            int total=page==P_MAPS?map_count:def.channel_count;
            if(list_pick>=total)list_pick=total-1;
            if(page==P_MAPS&&list_pick>=0)open_map(map_from_row(list_pick));
        }
        if(page==P_MAPS&&(down&KEY_A))open_map(map_from_row(list_pick));
        if(page==P_LIVE){
            if(down&KEY_L)active_slot=(active_slot+SLOTS-1)%SLOTS;
            if(down&KEY_R)active_slot=(active_slot+1)%SLOTS;
            if((down&KEY_X)&&list_pick>=0&&list_pick<def.channel_count){
                const BtField*f=&def.channels[list_pick];
                if(f->scaled&&f->width*f->height==1){
                    snprintf(channel_names[active_slot],sizeof(channel_names[active_slot]),"%s",f->name);
                    clear_history(active_slot);
                    refresh_idle_gauges();
                    if(sd_ready)(void)bt_sd_gauges_save((const char (*)[64])channel_names);
                }
            }
        }
    }else if(page==P_GRAPH){
        if(down&KEY_B)goto_page(P_HOME);
        if(down&KEY_Y)graph_stats=!graph_stats;
        if(down&KEY_L)active_slot=(active_slot+SLOTS-1)%SLOTS;
        if(down&KEY_R)active_slot=(active_slot+1)%SLOTS;
        if(down&KEY_X)goto_page(P_LIVE);
        if((down&KEY_TOUCH)&&tp->py>=49&&tp->py<142){
            int n=tp->px>=128?1:0;
            if(n>=0&&n<SLOTS)active_slot=n;
        }
    }else if(page==P_SOUNDS){
        if(down&KEY_B){goto_page(P_HOME);return;}
        if((down&KEY_UP)&&sound_pick>0)sound_pick--;
        if((down&KEY_DOWN)&&sound_pick+1<BTS_SOUND_COUNT)sound_pick++;
        if((down&KEY_TOUCH)&&tp->py>=28&&tp->py<169){
            int first=sound_pick>5?sound_pick-5:0;
            int next=first+(tp->py-28)/20;
            if(next>=0&&next<BTS_SOUND_COUNT)sound_pick=next;
        }
        if(down&(KEY_A|KEY_TOUCH)){
            if(!bts_sfx_play(sound_pick,sd_ready))
                snprintf(note,sizeof(note),"%.85s",bts_sfx_status(sd_ready));
        }
    }else if(page==P_LINK){
        if(down&KEY_B)goto_page(P_HOME);
        if(((down&KEY_A)||((down&KEY_TOUCH)&&tp->py>=55&&tp->py<84)) && !sd_ready){
            /* libfat mount is synchronous. This is a deliberate user action,
             * NEVER part of early boot. Draw the diagnostic stage BEFORE
             * calling the DLDI/SD driver so a stalled mount cannot produce
             * the old unexplained blank-screen-at-launch regression. */
            sd_attempted=true;
            snprintf(status_msg,sizeof(status_msg),"MOUNTING SD / WAIT FOR DRIVER");
            last_net_error[0]=0;
            render();
            swiWaitForVBlank();present();
            sd_ready=bt_sd_init();
            if(sd_ready){
                loading_cache=bt_cache_begin(&def);
                cache_loaded=false;
                snprintf(status_msg,sizeof(status_msg),loading_cache?
                    "SD CACHE / READING ECU INI":"SD READY / CHECKING ECU");
                if(!loading_cache){want_status=true;if(ecu_up)want_ini=true;}
            }else{
                snprintf(last_net_error,sizeof(last_net_error),"%.78s",bt_sd_message());
                snprintf(status_msg,sizeof(status_msg),"SD ACCESS FAILED / WIFI STILL USABLE");
            }
            return;
        }
        if(down&KEY_X){
            if(wifi_initialized){
                bt_async_cancel();bt_async_forget_connection();loading_ini=false;want_ini=false;
                bridge_up=ecu_up=false;ecu_status_known=false;set_compat();
                network_failure_count=0;network_resume_frame=0;last_socket_begin_frame=-1000;
        live_failure_streak=0;live_resume_frame=0;
                Wifi_DisconnectAP();Wifi_AutoConnect();
            }
            wifi_attempt=frame;snprintf(status_msg,sizeof(status_msg),"RECONNECTING SAVED WIFI");
        }
        if(down&KEY_Y){
            bt_async_cancel();bt_async_forget_connection();loading_ini=false;ini_hash_mismatch=false;ini_retry_frame=0;want_status=true;
            network_failure_count=0;network_resume_frame=0;last_socket_begin_frame=-1000;
        live_failure_streak=0;live_resume_frame=0;
            if(ecu_up&&!def.valid)want_ini=true;
            last_net_error[0]=0;
        }
    }
}
int main(void){
    videoSetMode(MODE_5_2D);videoSetModeSub(MODE_5_2D);
    vramSetBankA(VRAM_A_MAIN_BG);vramSetBankC(VRAM_C_SUB_BG);
    int a=bgInit(3,BgType_Bmp16,BgSize_B16_256x256,0,0);
    int b=bgInitSub(3,BgType_Bmp16,BgSize_B16_256x256,0,0);
    lcd_top=(uint16_t*)bgGetGfxPtr(a);lcd_bottom=(uint16_t*)bgGetGfxPtr(b);
    bt_ini_reset(&def);
    soundEnable();
    /* Draw a real frame BEFORE filesystem mount. Unlaunch + DSi SD was
     * confirmed to work on this device. Cache is loaded immediately if
     * present: no need to re-parse INI or wait for Wi-Fi to show menus. */
    page=P_HOME;
    snprintf(status_msg,sizeof(status_msg),"OPENING SAVED ECU TUNER");
    render();swiWaitForVBlank();present();
    sd_attempted=true;
    sd_ready=bt_sd_init();
    if(sd_ready){
        (void)bt_sd_gauges_load(channel_names);
        gauge_layout=bt_sd_layout_load();
        cache_loaded=bt_sd_snapshot_load(&def);
        if(cache_loaded){
            build_map_index();set_compat();
            snprintf(status_msg,sizeof(status_msg),"ECU INI READY FROM SD");
        }else{
            loading_cache=bt_cache_begin(&def);
            snprintf(status_msg,sizeof(status_msg),loading_cache?
                "READING SD ECU DEFINITION":"NO SD INI / WAIT FOR ECU");
        }
    }else{
        /* Preserve the visible UI and manual SD retry in LINK. */
        snprintf(status_msg,sizeof(status_msg),"SD NOT READY / CHECK LINK");
    }
    /* INIT_ONLY avoids a several-second blocking WFC_CONNECT at boot.
     * Autoconnect can subsequently associate in the normal render loop. */
    wifi_initialized=Wifi_InitDefault(INIT_ONLY);
    if(wifi_initialized){
        Wifi_AutoConnect();wifi_attempt=frame;
        snprintf(status_msg,sizeof(status_msg),"LOOKING FOR SAVED WIFI / TARGET " EXPECTED_AP);
    }else snprintf(status_msg,sizeof(status_msg),"WIFI INIT FAILED - CHECK DSI SETTINGS");
    while(pmMainLoop()){
        frame++;
        scanKeys();uint32_t down=keysDown(),held=keysHeld();touchPosition tp={0};
        if(down&KEY_TOUCH)touchRead(&tp);
        if(down&KEY_START)break;
        if(frame%20==0){
            update_wifi();
            if(!wifi_up&&wifi_initialized&&frame-wifi_attempt>=600){
                Wifi_AutoConnect();wifi_attempt=frame;
                snprintf(status_msg,sizeof(status_msg),"RETRYING SAVED WIFI / TARGET " EXPECTED_AP);
            }
        }
        /* Every D-pad/touch/button action resets the idle timer, including
         * navigation that does not open a new page. */
        if(down||held)activity_frame=frame;
        handle_buttons(down,held,&tp);
        update_idle();
        /* One-inflight HTTP; process small nonblocking steps after input. */
        service_network();
        if(frame%60==0){measured_hz=live_count;live_count=0;}
        bool draw_now=frame%2==0||down;
        if(draw_now)render();
        /* Exactly one VBlank per tick. The former render() had a second
         * wait every other tick, accidentally slowing HTTP polling. */
        swiWaitForVBlank();
        if(draw_now)present();
    }
    bt_async_cancel();bt_async_forget_connection();
    bts_sfx_shutdown();
    return 0;
}
