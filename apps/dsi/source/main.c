/* Brick Tuner Suite DSi v2.8
 * Native two-screen UI, sourced from the original 3DS v62 INI/BrickOS contract.
 * A DSi cannot use v62's Citro3D GPU API, but v28 adds a software 3D
 * shaded surface renderer for actual INI Z tables and retains a recognizable
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
#include <ctype.h>
#include <math.h>
#include "tuner.h"
#include "surface3d.h"
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
/* UI font upgrade: 9x14 glyphs for browsable tuning folders and settings.
 * 6x9 stays reserved for map-cell numbers and footers. */
static bool map_surface_3d=false;
static double map_orbit=0.62, map_elevation=0.72;
static int ref_bucket_head[1024],ref_bucket_tail[1024],ref_chain[BTS_MAX_REFS];
static int ref_index_count=-1;
static const BtDialogRef *ref_index_base=NULL;
static int ref_index_signature=0;
static unsigned ref_index_builds=0;
/* Reuse expensive SD-derived computed output formulas across nested panels. */
#define BTS_GAUGE_FORMULA_CACHE 48
typedef struct {char channel[64];char expression[256];bool valid;} BtCachedFormula;
static BtCachedFormula gauge_formula_cache[BTS_GAUGE_FORMULA_CACHE];
static int gauge_formula_count=0;
static int dialog_gauge_page=0;
static int dialog_visible_ids[512],dialog_visible_total=0;
/* Cached per opened dialog, never traverse the full 2,799-ref INI on VBlank. */
static int dialog_gauge_ref_ids[64],dialog_gauge_total=0;
static char dialog_gauge_expr[64][256];
static bool dialog_gauge_expr_valid[64];
static BtFavorite favorites[BTS_MAX_FAVORITES];
static int favorite_count=0;
static char favorite_signature[160]="";
static bool favorites_loaded=false, editor_readonly=false;
static bool view_from_tree=false;
static int active_ini_graph=-1, active_ini_graph_line=0;
static char graph_from_dialog[64]="", graph_from_label[76]="";
static bool graph_from_ini=false;
static const BtMap* selected_map(void);
static void load_favorites(void);
static void toggle_favorite(const char *target,const char *label);
static bool is_favorite(const char *target);

/* 2/3/4 top gauges; default remains 2 AFR/VBAT. L/R only on HOME. */
static int gauge_layout=2;
static int sound_pick=0;
static int map_indices[BTS_MAX_MAPS],map_count=0,chosen_map=-1;
static int cx=0,cy=0,active_slot=0;
static int map_focus=0; /* 0=Z calibration, 1=X axis, 2=Y axis */
/* DSi v2.3 opens ECU calibration views without blocking VBlank/touch.
 * Three bounded HTTP/TS R stages: actual Z bytes, optional X/Y axes.
 * Writes are separate, explicitly confirmed bench-only transactions. */
typedef struct {
    bool active;
    int stage, offset, total, chunk, map_id;
    uint8_t axis_raw[512];
} BtCalLoad;
static BtCalLoad cal_load;
static BtField active_fields[3];
static BtIniFormat active_formats[3];
static bool active_field_present[3];
static char trace_formula[2][256];
static bool trace_formula_valid[2];
static char channel_names[SLOTS][64]={"AFRValue","VBatt"};
static char graph_formula[SLOTS][256];
static bool graph_formula_valid[SLOTS];
/* Graph panels may have more than two INI channels. Keep the visible eight
 * resolved, including computed OutputChannels, without a 640-line RAM cache. */
static char graph_list_names[8][64],graph_list_formulas[8][256];
static bool graph_list_valid[8];
static int graph_list_start=0;
static void refresh_graph_formulas(void){
    memset(graph_formula_valid,0,sizeof(graph_formula_valid));
    memset(graph_list_valid,0,sizeof(graph_list_valid));
    for(int i=0;i<SLOTS;i++){
        if(sd_ready && !bt_field(&def,channel_names[i],true))
            graph_formula_valid[i]=bt_ini_rhs(bt_sd_cache_path(),"OutputChannels",
                        channel_names[i],graph_formula[i],sizeof(graph_formula[i]));
    }
    if(!graph_from_ini||active_ini_graph<0||active_ini_graph>=def.graph_count)return;
    const BtIniGraph *g=&def.graphs[active_ini_graph];
    graph_list_start=(active_ini_graph_line/8)*8;
    for(int i=0;i<8;i++){
        graph_list_names[i][0]=0;
        int index=graph_list_start+i;
        if(index>=g->line_count||g->first_line+index>=def.graph_line_count)break;
        const char *name=def.graph_lines[g->first_line+index].channel;
        snprintf(graph_list_names[i],sizeof(graph_list_names[i]),"%.63s",name);
        if(sd_ready && !bt_field(&def,name,true))
            graph_list_valid[i]=bt_ini_rhs(bt_sd_cache_path(),"OutputChannels",
                name,graph_list_formulas[i],sizeof(graph_list_formulas[i]));
    }
}
static int expression_live_length(const char *expr,int existing){
    if(!expr||!*expr)return existing;
    for(const char *q=expr;*q;){
        if(!(isalpha((unsigned char)*q)||*q=='_')){q++;continue;}
        char name[64];int n=0;
        while(isalnum((unsigned char)*q)||*q=='_'){
            if(n<63)name[n++]=*q;
            q++;
        }
        name[n]=0;
        const BtField *f=bt_field(&def,name,true);
        if(f&&f->scaled){
            int end=f->offset+bt_type_size(f->type);
            if(end>existing&&end<=BTS_LIVE_CAP)existing=end;
        }
    }
    return existing;
}
static const char *mode_names[6]={
    "useMetricOnInterface","useLambdaOnInterface","fuelAlgorithm",
    "veOverrideMode","afrOverrideMode","ignOverrideMode"
};
static double mode_values[6];
static bool mode_valid[6];
static bool lookup_config_variable(const char *name,void *ctx,double *value){
    (void)ctx;
    for(int i=0;i<6;i++)if(!strcasecmp(name,mode_names[i])){
        if(!mode_valid[i])return false;
        *value=mode_values[i];return true;
    }
    return false;
}
static void resolve_active_formats(void){
    for(int i=0;i<3;i++){
        if(!active_field_present[i])continue;
        BtField *field=&active_fields[i];BtIniFormat *f=&active_formats[i];
        if(field->kind==2)continue;
        double scale=0,shift=0,low=0,high=0;
        if(bt_numeric_eval(f->scale,lookup_config_variable,NULL,&scale)&&scale!=0 &&
           bt_numeric_eval(f->translate,lookup_config_variable,NULL,&shift)){
            field->scale=scale;field->translate=shift;field->scaled=true;
        }
        if(bt_numeric_eval(f->low,lookup_config_variable,NULL,&low) &&
           bt_numeric_eval(f->high,lookup_config_variable,NULL,&high)&&low<=high){
            field->low=low;field->high=high;field->limits_known=true;
        }
    }
}

/* Baseline and edit memory are held fixed until the cooperative protected
 * transaction reports verified readback or fails closed. */
static uint8_t *write_baseline=NULL,*write_edited=NULL;
static int write_length=0,write_focus=0;
static void calibration_service(void);
static const BtField *focused_cal_field(void);
static double focused_value(void);
static bool any_map_dirty(void);


static double hist[SLOTS][HISTORY],low[SLOTS],high[SLOTS],sum[SLOTS];
static bool hist_valid[SLOTS][HISTORY];
static unsigned sample_count[SLOTS];
static int history_at=0;
static bool graph_stats=false;
static char dialog_name[64]="",dialog_label[76]="";
static char dialog_owner_stack[6][64],dialog_label_stack[6][76];
static int dialog_depth=0;
static int dialog_pick=0,dialog_scroll=0;
static bool dialog_info=false;
static char dialog_context[1024];
static char editor_description[1024];
static bool editor_help_visible=false;
static bool virtual_z_map=false; /* INI UI-only values, never an ECU memory write */
static int dialog_help_index=-1,dialog_help_page=0;

static const double steps[]={0.1,1.0,5.0,10.0};
static int step_index=1;
static bool abandon_confirm=false;
static bool keypad_open=false;
static char keypad_input[24]="";
/* Dynamic INI GPIO/input/output dropdowns routinely contain 100+ entries. */
#define BTS_DSI_CHOICE_CAP 256
static int bit_choice_count=0;
static char bit_choice_labels[BTS_DSI_CHOICE_CAP][36];
/* A filtered *display index* into the exact INI enum IDs. The raw ECU
 * numeric ID is never remapped, even when a list contains 200 holes. */
static int bit_visible_ids[BTS_DSI_CHOICE_CAP],bit_visible_count=0;
static void filter_bit_choices(void){
    bit_visible_count=0;
    for(int i=0;i<bit_choice_count;i++)
        if(bit_choice_labels[i][0])bit_visible_ids[bit_visible_count++]=i;
}
static bool bit_picker_open=false;
static int bit_picker_pick=0,bit_picker_scroll=0;
static const char *keypad_keys[16]={
    "7","8","9","DEL", "4","5","6","CLR",
    "1","2","3","SIGN", ".","0","OK","BACK"
};

enum {P_HOME,P_TUNE,P_LIVE,P_GRAPH,P_SOUNDS,P_LINK,P_FAV,P_COUNT};
static const char *tabs[P_COUNT]={"HOME","TUNE","VIEW","GRAPH","SOUNDS","LINK","FAV"};
/* The graph and realtime channel screens live only behind the INI View menu. */
static const int home_pages[4]={P_TUNE,P_FAV,P_SOUNDS,P_LINK};
static const int home_icons[4]={5,8,6,7};
static const char *home_labels[4]={"TUNING TREE","FAVORITE SETTINGS","SOUNDBOARD","CONNECTION"};
static void refresh_idle_gauges(void){
    bt_idle_select(&def,channel_names[0],channel_names[1],&idle_gauges);
}
static bool can_show_idle(void){
    /* Never obscure an editable map, an INI dialog or uncommitted changes. */
    return ini_ok && !loading_cache && !loading_ini && !table.valid &&
        !dialog_name[0] && (page==P_HOME || page==P_TUNE || page==P_FAV);
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
static void tiny_text(uint16_t*d,int x,int y,const char*s,uint16_t color,int max_px){
    /* 6 x 9 native small font for dense map grid / long menu descriptions. */
    if(!s||max_px<6)return;
    int cap=max_px/6;
    for(int i=0;s[i]&&i<cap&&x+i*6+6<=W;i++){
        unsigned char ch=(unsigned char)s[i];if(ch<32||ch>126)ch='?';
        for(int ry=0;ry<9;ry++){
            uint8_t bits=bts_glyphs[ch-32][ry];
            for(int rx=0;rx<6;rx++)if(bits&(1u<<rx))pix(d,x+i*6+rx,y+ry,color);
        }
    }
}
static void tiny_fit(uint16_t*d,int x,int y,const char*s,uint16_t color,int width){
    if(!s||width<12)return;
    int cap=width/6;
    int n=(int)strlen(s);
    if(n<=cap){tiny_text(d,x,y,s,color,width);return;}
    char b[64];if(cap>(int)sizeof(b)-1)cap=(int)sizeof(b)-1;
    int end=cap-2;if(end<1)return;
    memcpy(b,s,(size_t)end);b[end]='.';b[end+1]='.';b[end+2]=0;
    tiny_text(d,x,y,b,color,width);
}
static void text(uint16_t*d,int x,int y,const char*s,uint16_t c,int limit){
    if(!s)return;
    for(int i=0;s[i]&&i<limit&&x+i*9<W;i++)
        glyph(d,x+i*9,y,(unsigned char)s[i],c,1);
}
/* Native 9x14 menu text with predictable clipping and an explicit ellipsis.
 * No scaling/blurring: this uses the existing font at its intended size. */
static void menu_text(uint16_t*d,int x,int y,const char*s,uint16_t color,int width){
    if(!s||width<9)return;
    int cap=width/9,n=(int)strlen(s),draw=n<cap?n:cap;
    if(n>cap&&cap>=3)draw=cap-2;
    for(int i=0;i<draw;i++)glyph(d,x+i*9,y,(unsigned char)s[i],color,1);
    if(n>cap&&cap>=3){glyph(d,x+draw*9,y,'.',color,1);
        glyph(d,x+(draw+1)*9,y,'.',color,1);}
}
/* Draw complete INI help in readable wrapped lines, including URLs and long
 * constant expressions that do not contain spaces. No fabricated prose. */
static void text_wrapped(uint16_t *d,int x,int y,const char *src,uint16_t color,int columns,int lines){
    if(!src||!*src||columns<2||columns>28)return;
    while(*src&&lines-->0){
        while(*src==' '||*src=='\t'||*src=='\n'||*src=='\r')src++;
        if(!*src)break;
        int len=0,last_space=0;
        while(src[len]&&src[len]!='\n'&&len<columns){
            if(src[len]==' ')last_space=len;
            ++len;
        }
        if(len==columns && src[len] && src[len]!='\n'&&last_space>0)len=last_space;
        char line[30];memcpy(line,src,(size_t)len);line[len]=0;
        text(d,x,y,line,color,columns);y+=16;src+=len;
        if(*src=='\n')src++;
    }
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
static bool computed_value_lookup(const char *name,void *ctx,double *value){
    (void)ctx;
    if(lookup_config_variable(name,NULL,value))return true;
    const BtField *field=bt_field(&def,name,true);
    return field&&bt_field_value(&def,field,live.bytes,live.length,0,value);
}
static bool get_value(const char*name,double*value){
    if(!fresh()||!name||!value)return false;
    const BtField*f=bt_field(&def,name,true);
    if(f&&bt_field_value(&def,f,live.bytes,live.length,0,value))return true;
    for(int slot=0;slot<SLOTS;slot++){
        if(graph_formula_valid[slot]&&!strcasecmp(name,channel_names[slot]))
            return bt_numeric_eval(graph_formula[slot],computed_value_lookup,NULL,value);
    }
    for(int i=0;i<8;i++)if(graph_list_valid[i]&&
       !strcasecmp(name,graph_list_names[i]))
        return bt_numeric_eval(graph_list_formulas[i],computed_value_lookup,NULL,value);
    for(int i=0;i<2;i++){
        const BtMap*m=selected_map();
        const char *axis=m?(i==0?m->xchan:m->ychan):NULL;
        if(axis&&!strcasecmp(name,axis)&&trace_formula_valid[i])
            return bt_numeric_eval(trace_formula[i],computed_value_lookup,NULL,value);
    }
    return false;
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
    load_favorites();
    map_count=0;
    for(int i=0;i<def.map_count;i++){
        const BtField*f=bt_field(&def,def.maps[i].z,false);
        if(f&&bt_type_size(f->type)>0&&f->page>=1&&f->page<=64&&
           f->width>0&&f->height>0&&f->width*f->height<=BTS_MAP_CELLS)
            map_indices[map_count++]=i;
    }
}
static const BtMap* selected_map(void){
    if(chosen_map==BTS_MAX_MAPS)return &scalar_map;
    if(chosen_map>=0&&chosen_map<def.map_count)return &def.maps[chosen_map];
    return NULL;
}
static void load_favorites(void){
    if(!def.valid||!sd_ready)return;
    if(favorites_loaded&&!strcmp(favorite_signature,def.signature))return;
    favorite_count=bt_sd_favorites_load(def.signature,favorites,BTS_MAX_FAVORITES);
    snprintf(favorite_signature,sizeof(favorite_signature),"%s",def.signature);
    favorites_loaded=true;
}
static bool is_favorite(const char *target){
    if(!target||!*target)return false;
    for(int i=0;i<favorite_count;i++)if(!strcasecmp(target,favorites[i].target))return true;
    return false;
}
static void toggle_favorite(const char *target,const char *label){
    if(!target||!*target||!label||!*label)return;
    load_favorites();
    for(int i=0;i<favorite_count;i++)if(!strcasecmp(target,favorites[i].target)){
        memmove(&favorites[i],&favorites[i+1],(size_t)(favorite_count-i-1)*sizeof(favorites[0]));
        favorite_count--;
        snprintf(note,sizeof(note),"REMOVED FAVORITE: %.64s",label);
        if(sd_ready)bt_sd_favorites_save(def.signature,favorites,favorite_count);
        return;
    }
    if(favorite_count>=BTS_MAX_FAVORITES){snprintf(note,sizeof(note),"FAVORITES FULL (48)");return;}
    BtFavorite *f=&favorites[favorite_count++];memset(f,0,sizeof(*f));
    snprintf(f->target,sizeof(f->target),"%s",target);
    snprintf(f->label,sizeof(f->label),"%s",label);
    if(sd_ready)bt_sd_favorites_save(def.signature,favorites,favorite_count);
    snprintf(note,sizeof(note),"FAVORITED: %.72s",label);
}
static int next_curve_series(int current){
    if(current<0||current>=def.map_count||!def.maps[current].curve)return -1;
    const char *name=def.maps[current].name;
    int first=-1;
    for(int i=0;i<def.map_count;i++)if(def.maps[i].curve&&!strcasecmp(name,def.maps[i].name)){
        if(first<0)first=i;
        if(i>current)return i;
    }
    return first!=current?first:-1;
}
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
    /* Virtual axes have exact INI defaults but no ECU field; they are valid
     * for graph positioning even though they cannot be written. */
    if(!m||!table.valid||!table.has_x||!m->xchan[0]||
       (active_field_present[1]&&!active_fields[1].scaled))return;
    double x=0,y=0;
    if(!get_value(m->xchan,&x))return;
    int tx=nearest_bin(x,table.xbins,table.width);
    if(tx<0)return;
    int ty=0;
    if(table.height>1){
        if(!table.has_y||!m->ychan[0]||
           (active_field_present[2]&&!active_fields[2].scaled)||
           !get_value(m->ychan,&y))return;
        ty=nearest_bin(y,table.ybins,table.height);
        if(ty<0)return;
    }
    trace_x=tx;trace_y=ty;trace_valid=true;
}
static uint16_t heat_color(double v,double min,double max){
    double t=max>min?(v-min)/(max-min):0.5;
    if(!isfinite(t))t=0.5;
    if(t<0)t=0;
    if(t>1)t=1;
    double r=0,g=0,b=0;
    if(t<0.25){double a=t*4;r=4;g=9+a*12;b=22-a*6;}
    else if(t<0.5){double a=(t-0.25)*4;r=4+a*7;g=21+a*7;b=16-a*10;}
    else if(t<0.75){double a=(t-0.5)*4;r=11+a*17;g=28-a*5;b=6-a*4;}
    else {double a=(t-0.75)*4;r=28;g=23-a*14;b=2+a*4;}
    return COL((int)r,(int)g,(int)b);
}
static void map_number(char *dst,size_t cap,double value){
    if(fabs(value)>=1000)snprintf(dst,cap,"%.0f",value);
    else if(fabs(value)>=100)snprintf(dst,cap,"%.0f",value);
    else if(fabs(value)>=10)snprintf(dst,cap,"%.1f",value);
    else snprintf(dst,cap,"%.2f",value);
}
static void map_top(void){
    draw_header(table.dirty?"MAP / STAGED CHANGES":"ECU CALIBRATION MAP");
    if(cal_load.active){
        const BtMap *loading=selected_map();
        text(top,11,44,loading?loading->label:"ECU CALIBRATION",HIGHLIGHT,30);
        const char *stage=cal_load.stage==0?"Z CALIBRATION":cal_load.stage==1?"X AXIS":"Y AXIS";
        text(top,11,79,"READING ECU / NONBLOCKING",WHITE,28);
        text(top,11,101,stage,CYAN,28);
        char progress[44];
        snprintf(progress,sizeof(progress),"%d / %d BYTES",cal_load.offset,cal_load.total);
        text(top,11,123,progress,WHITE,28);
        text(top,11,160,"B: CANCEL / ECU UNCHANGED",MUTED,27);
        return;
    }
    if(chosen_map<0||!table.valid){
        text(top,11,45,"REAL ECU DEFINITION",HIGHLIGHT,32);
        text(top,11,64,"MAPS AND CURVES",WHITE,32);
        text(top,11,105,"SELECT A MAP BELOW",MUTED,32);
        text(top,11,132,"A / TOUCH TO VIEW",MUTED,32);
        return;
    }
    const BtMap*m=selected_map();if(!m)return;
    map_live_trace(m);
    tiny_fit(top,11,33,m->label,HIGHLIGHT,m->curve?182:236);
    if(chosen_map==BTS_MAX_MAPS && table.width==1 && table.height==1){
        const BtField *f=focused_cal_field();
        text(top,11,53,f&&f->kind==2?"INI PREDEFINED SELECTION":"INI NUMERIC SCALAR",CYAN,28);
        char val[52];number(focused_value(),val,sizeof(val));
        text(top,11,82,val,WHITE,20);
        if(f&&f->kind==2){
            int index=(int)focused_value();
            if(index>=0&&index<bit_choice_count&&bit_choice_labels[index][0])
                text_wrapped(top,11,109,bit_choice_labels[index],HIGHLIGHT,26,2);
            else text(top,11,109,"OPTION NAME UNAVAILABLE",RED,26);
        }else if(f){
            text(top,11,108,f->unit,CYAN,27);
            if(f->limits_known){
                snprintf(val,sizeof(val),"INI RANGE %.4g TO %.4g",f->low,f->high);
                text(top,11,132,val,MUTED,27);
            }
        }
        text(top,11,168,editor_readonly?"INI READ ONLY":"ECU VALUE / STAGED EDIT",MUTED,26);
        return;
    }
    if(virtual_z_map)tiny_text(top,9,176,"INI DEFAULT CURVE / READ ONLY",HIGHLIGHT,240);
    if(m->curve){
        char series[26];snprintf(series,sizeof(series),"CURVE %d",m->series_index+1);
        tiny_text(top,202,31,series,CYAN,54);
    }
    if(map_focus)tiny_text(top,176,31,map_focus==1?"X EDIT":"Y EDIT",CYAN,75);
    char labels[90];
    snprintf(labels,sizeof(labels),"X %.17s   Y %.17s",
             m->x_label[0]?m->x_label:m->x,m->y_label[0]?m->y_label:m->y);
    tiny_fit(top,9,48,labels,MUTED,241);
    if(map_surface_3d&&table.width>1&&table.height>1){
        BtSurface3D view={0};
        view.values=table.cells;view.xbins=table.xbins;view.ybins=table.ybins;
        view.width=table.width;view.height=table.height;
        view.has_x=table.has_x&&table.x_count>=table.width;
        view.has_y=table.has_y&&table.y_count>=table.height;
        view.yaw=map_orbit;view.elevation=map_elevation;
        view.selected_x=cx;view.selected_y=cy;
        view.trace_valid=trace_valid;view.trace_x=trace_x;view.trace_y=trace_y;
        bt_surface3d_draw(top,&view);
        if(table.dirty)tiny_text(top,198,49,"STAGED",HIGHLIGHT,55);
        tiny_fit(top,8,181,trace_valid?"3D SURFACE / LIVE TRACE ACTIVE":
                  "3D SURFACE / L + DPAD TO ORBIT",trace_valid?GREEN:CYAN,242);
        return;
    }
    int cols=table.width<5?table.width:5,rows=table.height<5?table.height:5;
    int left=cx-cols/2,above=cy-rows/2;
    if(left<0)left=0;
    if(above<0)above=0;
    if(left+cols>table.width)left=table.width-cols;
    if(above+rows>table.height)above=table.height-rows;
    double minv=INFINITY,maxv=-INFINITY;
    for(int i=0;i<table.width*table.height;i++){
        double v=table.cells[i];if(!isfinite(v))continue;
        if(v<minv)minv=v;
        if(v>maxv)maxv=v;
    }
    if(!isfinite(minv)||!isfinite(maxv)){minv=0;maxv=1;}
    int cellw=42,cellh=18,dx=43,dy=20;
    int x0=37+(5-cols)*dx/2,y0=61;
    for(int yy=0;yy<rows;yy++){
        int ty=above+yy,py=y0+yy*dy;
        if(table.has_y && ty<table.y_count){
            char v[22];map_number(v,sizeof(v),table.ybins[ty]);
            tiny_fit(top,2,py+4,v,MUTED,33);
        }else if(table.height>1)tiny_text(top,2,py+4,"?",MUTED,22);
        for(int xx=0;xx<cols;xx++){
            int tx=left+xx,px=x0+xx*dx;
            bool selected=tx==cx && ty==cy;
            bool traced=trace_valid&&tx==trace_x&&ty==trace_y;
            if(selected||traced)round_fill(top,px-1,py-1,cellw+2,cellh+2,3,
                                           selected?HIGHLIGHT:CYAN);
            uint16_t tile=heat_color(table.cells[ty*table.width+tx],minv,maxv);
            round_fill(top,px,py,cellw,cellh,3,tile);
            char v[24];map_number(v,sizeof(v),table.cells[ty*table.width+tx]);
            tiny_fit(top,px+3,py+5,v,WHITE,cellw-5);
            if(traced){fill(top,px+cellw-5,py+2,3,3,WHITE);}
        }
    }
    if(table.has_x){
        for(int xx=0;xx<cols;xx++){
            int tx=left+xx;char v[22];map_number(v,sizeof(v),table.xbins[tx]);
            tiny_fit(top,x0+xx*dx+2,164,v,CYAN,cellw-2);
        }
    }else tiny_text(top,38,164,"NO INI X AXIS",MUTED,125);
    char line[80];
    if(trace_valid)snprintf(line,sizeof(line),"TRACE %d,%d  /  CELL %d,%d",
                  trace_x+1,trace_y+1,cx+1,cy+1);
    else if(table.has_x && (table.height==1||table.has_y))
        snprintf(line,sizeof(line),"TRACE: WAITING FOR INI OUTPUT DATA");
    else snprintf(line,sizeof(line),"AXIS INI DEFINITION UNAVAILABLE");
    tiny_fit(top,8,181,line,trace_valid?GREEN:MUTED,246);
    if(table.dirty)tiny_text(top,180,48,"STAGED",HIGHLIGHT,64);
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
/* TS border-layout containers named ...West/...East/...Center are
 * presentation-only wrappers. Flatten those into the single navigable list,
 * retaining their real child controls, order, and identifiers. */
static bool panel_layout_wrapper(const BtDialogRef *r){
    if(r->kind!=1||r->condition[0])return false;
    if(r->placement[0])return true;
    const char *s=r->name;size_t n=strlen(s);
    static const char *suffixes[]={"West","East","Center","North","South"};
    for(int k=0;k<5;k++){
        size_t len=strlen(suffixes[k]);
        if(n>len&&!strcasecmp(s+n-len,suffixes[k]))return true;
    }
    return false;
}
/* A dialog could have thousands of referenced controls, but only a handful
 * belong to its owner. Build a bucket-linked index in original INI order once,
 * then reuse it for folder traversal and gauge lookup. */
static unsigned owner_bucket(const char *owner){
    unsigned hash=2166136261u;
    for(const unsigned char *p=(const unsigned char *)owner;*p;p++){
        hash^=(unsigned char)tolower(*p);hash*=16777619u;
    }
    return hash&1023u;
}
static void reset_ref_index(void){
    ref_index_count=-1;ref_index_base=NULL;ref_index_signature++;
    gauge_formula_count=0;
    bt_ini_help_index_reset();
}
static void ensure_ref_index(void){
    if(ref_index_count==def.ref_count&&ref_index_base==def.refs)return;
    for(int b=0;b<1024;b++){ref_bucket_head[b]=-1;ref_bucket_tail[b]=-1;}
    for(int i=0;i<def.ref_count&&i<BTS_MAX_REFS;i++){
        unsigned b=owner_bucket(def.refs[i].owner);
        ref_chain[i]=-1;
        if(ref_bucket_tail[b]>=0)ref_chain[ref_bucket_tail[b]]=i;
        else ref_bucket_head[b]=i;
        ref_bucket_tail[b]=i;
    }
    ref_index_base=def.refs;ref_index_count=def.ref_count;
    ref_index_builds++;
}
static int owner_first(const char *owner){
    ensure_ref_index();return ref_bucket_head[owner_bucket(owner)];
}
/* Gauge-only panels are displayed above the editor; they are not fields.
 * Depth bounded and all owner searches now use the sparse index. */
static bool gauge_only_panel(const char *owner,int depth){
    if(depth>=7)return false;
    bool found=false;
    for(int i=owner_first(owner);i>=0;i=ref_chain[i]){
        const BtDialogRef *r=&def.refs[i];
        if(strcasecmp(r->owner,owner))continue; /* hash collision */
        if(r->kind==7){found=true;continue;}
        if(r->kind==1&&strcasecmp(r->name,owner)&&
           gauge_only_panel(r->name,depth+1)){found=true;continue;}
        return false;
    }
    return found;
}
static void dialog_entry_find(const char *owner,int depth){
    if(depth>=7||dialog_visible_total>=512)return;
    for(int i=owner_first(owner);i>=0;i=ref_chain[i]){
        const BtDialogRef *r=&def.refs[i];
        if(strcasecmp(r->owner,owner)||r->kind==7)continue;
        if(r->kind==1&&gauge_only_panel(r->name,0))continue;
        if(panel_layout_wrapper(r)&&strcasecmp(r->owner,r->name))
            dialog_entry_find(r->name,depth+1);
        else if(dialog_visible_total<512)
            dialog_visible_ids[dialog_visible_total++]=i;
        if(dialog_visible_total>=512)return;
    }
}
static void rebuild_dialog_entries(void){
    dialog_visible_total=0;
    if(dialog_name[0])dialog_entry_find(dialog_name,0);
}
static int dialog_count(void){return dialog_visible_total;}
static void open_map(int id);
static int dialog_at(int row){
    return row>=0&&row<dialog_visible_total?dialog_visible_ids[row]:-1;
}
static void rebuild_dialog_gauges(void);
static void open_dialog(const char*id,const char*label){
    if(dialog_name[0]){
        if(dialog_depth>=6){snprintf(note,sizeof(note),"MAX NESTED PANEL DEPTH");return;}
        snprintf(dialog_owner_stack[dialog_depth],sizeof(dialog_owner_stack[0]),"%s",dialog_name);
        snprintf(dialog_label_stack[dialog_depth],sizeof(dialog_label_stack[0]),"%s",dialog_label);
        dialog_depth++;
    }
    snprintf(dialog_name,sizeof(dialog_name),"%s",id);
    snprintf(dialog_label,sizeof(dialog_label),"%s",label);
    dialog_pick=dialog_scroll=0;dialog_info=false;dialog_gauge_page=0;
    dialog_help_index=-1;dialog_help_page=0;
    rebuild_dialog_entries();
    rebuild_dialog_gauges();
}
static bool open_ini_field(const char *target,const char *label,bool readonly){
    const BtField *f=bt_field(&def,target,false);
    if(!f||bt_type_size(f->type)<1||f->width*f->height<1||
       f->width*f->height>BTS_MAP_CELLS)return false;
    bit_choice_count=bit_visible_count=0;
    if(f->kind==2&&sd_ready){
        bit_choice_count=bt_bit_choices(bt_sd_cache_path(),f->name,bit_choice_labels,BTS_DSI_CHOICE_CAP);
        filter_bit_choices();
    }
    memset(&scalar_map,0,sizeof(scalar_map));
    snprintf(scalar_map.label,sizeof(scalar_map.label),"%s",label&&*label?label:f->name);
    snprintf(scalar_map.z,sizeof(scalar_map.z),"%s",f->name);
    editor_readonly=readonly||f->read_only||(f->kind==2&&!bit_visible_count);
    bit_picker_open=false;
    open_map(BTS_MAX_MAPS);
    if(f->kind==2&&!bit_visible_count)
        snprintf(note,sizeof(note),"INI OPTIONS UNRESOLVED / READ ONLY");
    return true;
}
static void goto_page(int next);
static void open_ini_graph(const char *id){
    int index=-1;
    for(int i=0;i<def.graph_count;i++)if(!strcasecmp(def.graphs[i].name,id)){index=i;break;}
    if(index<0){snprintf(note,sizeof(note),"INI GRAPH NOT DEFINED");return;}
    const BtIniGraph *g=&def.graphs[index];
    if(!g->line_count){snprintf(note,sizeof(note),"INI GRAPH HAS NO CHANNELS");return;}
    for(int slot=0;slot<SLOTS;slot++){
        int line=g->first_line+(slot%g->line_count);
        const char *channel=def.graph_lines[line].channel;
        snprintf(channel_names[slot],sizeof(channel_names[slot]),"%s",channel);
        clear_history(slot);
    }
    snprintf(graph_from_dialog,sizeof(graph_from_dialog),"%s",dialog_name);
    snprintf(graph_from_label,sizeof(graph_from_label),"%s",dialog_label);
    graph_from_ini=true;active_ini_graph=index;active_ini_graph_line=0;
    refresh_graph_formulas();
    view_from_tree=true;
    goto_page(P_GRAPH);
}
static void dialog_action(void){
    int index=dialog_at(dialog_pick);if(index<0||!ini_ok)return;
    const BtDialogRef*r=&def.refs[index];
    if(r->kind==6){open_ini_graph(r->name);return;}
    if(r->kind==1){
        /* Built-in TunerStudio panels are not a set of editable fields in
         * the INI; do not invent controls for such a panel. */
        int children=0;
        for(int i=owner_first(r->name);i>=0;i=ref_chain[i])
            if(!strcasecmp(def.refs[i].owner,r->name)){children=1;break;}
        if(children){bts_sfx_play(bts_sound_for_entry(r->label,r->name),sd_ready);
            open_dialog(r->name,r->label);}
        else snprintf(note,sizeof(note),"TS BUILT-IN PANEL / NO INI CONTROLS");
        return;
    }
    if(r->kind==2){
        snprintf(note,sizeof(note),"INI COMMAND %s / NOT IMPLEMENTED",r->name);return;
    }
    if(r->kind==5){
        snprintf(note,sizeof(note),"INI INFO: %.78s",r->label);return;
    }
    if(r->kind==4){
        double v=0;
        if(get_value(r->name,&v)){
            char b[36];number(v,b,sizeof(b));
            snprintf(note,sizeof(note),"%.45s: %.24s (LIVE)",r->name,b);
        }else snprintf(note,sizeof(note),"LIVE CHANNEL NOT AVAILABLE: %.50s",r->name);
        return;
    }
    int target_map=map_for_target(r->name);
    if(target_map>=0){editor_readonly=r->kind==3;open_map(target_map);return;}
    if(!open_ini_field(r->name,r->label,r->kind==3))
        snprintf(note,sizeof(note),"INI CONTROL HAS NO SUPPORTED ECU TYPE");
}
/* Each reference gauge is a template -> live OutputChannel. INI placement
 * separates reference/diagnostics (usually East/North) from settings
 * (usually West/Center). Keep one list below and gauges above. */
static void collect_dialog_gauges(const char *owner,int depth){
    if(depth>=7||dialog_gauge_total>=64)return;
    for(int i=owner_first(owner);i>=0;i=ref_chain[i]){
        const BtDialogRef *r=&def.refs[i];
        if(strcasecmp(r->owner,owner))continue;
        if(r->kind==7){
            if(dialog_gauge_total<64)
                dialog_gauge_ref_ids[dialog_gauge_total++]=i;
        }else if(r->kind==1&&strcasecmp(r->name,owner))
            collect_dialog_gauges(r->name,depth+1);
        if(dialog_gauge_total>=64)return;
    }
}
static void rebuild_dialog_gauges(void){
    dialog_gauge_total=0;
    memset(dialog_gauge_expr_valid,0,sizeof(dialog_gauge_expr_valid));
    if(!dialog_name[0])return;
    collect_dialog_gauges(dialog_name,0);
    for(int i=0;i<dialog_gauge_total;i++){
        const BtGauge*g=bt_gauge(&def,def.refs[dialog_gauge_ref_ids[i]].name);
        if(g&&sd_ready&&!bt_field(&def,g->channel,true)){
            int cached=-1;
            for(int k=0;k<gauge_formula_count;k++)
                if(!strcasecmp(gauge_formula_cache[k].channel,g->channel)){cached=k;break;}
            if(cached<0&&gauge_formula_count<BTS_GAUGE_FORMULA_CACHE){
                cached=gauge_formula_count++;
                BtCachedFormula *entry=&gauge_formula_cache[cached];
                snprintf(entry->channel,sizeof(entry->channel),"%s",g->channel);
                entry->valid=bt_ini_rhs(bt_sd_cache_path(),"OutputChannels",
                      g->channel,entry->expression,sizeof(entry->expression));
            }
            if(cached>=0){
                dialog_gauge_expr_valid[i]=gauge_formula_cache[cached].valid;
                if(dialog_gauge_expr_valid[i])snprintf(dialog_gauge_expr[i],
                    sizeof(dialog_gauge_expr[i]),"%s",gauge_formula_cache[cached].expression);
            }
        }
    }
}
static int dialog_gauge_count(void){return dialog_gauge_total;}
static int dialog_gauge_at(int index){
    return index>=0&&index<dialog_gauge_total?dialog_gauge_ref_ids[index]:-1;
}
static bool gauge_live_value(const BtGauge *g,double *out){
    if(!g||!fresh())return false;
    if(get_value(g->channel,out))return true;
    for(int i=0;i<dialog_gauge_total;i++){
        if(!dialog_gauge_expr_valid[i])continue;
        const BtGauge *other=bt_gauge(&def,def.refs[dialog_gauge_ref_ids[i]].name);
        if(other==g)return bt_numeric_eval(dialog_gauge_expr[i],
                                            computed_value_lookup,NULL,out);
    }
    return false;
}
static void draw_ref_gauge(const BtGauge *g,int x,int y,int width){
    if(!g)return;
    round_fill(top,x,y,width,36,4,PANEL);
    char label[92];snprintf(label,sizeof(label),"%s",g->label[0]?g->label:g->name);
    tiny_fit(top,x+6,y+4,label,WHITE,width-119);
    char value[52];double v=0;
    bool ok=gauge_live_value(g,&v);
    if(ok){char num[28];number(v,num,sizeof(num));
        snprintf(value,sizeof(value),"%.23s %.20s",num,g->unit);
    }else snprintf(value,sizeof(value),"-- %s",g->unit);
    tiny_fit(top,x+width-100,y+4,value,ok?GREEN:MUTED,96);
    fill(top,x+6,y+24,width-12,6,LITE);
    if(ok&&g->range_known){
        double f=(v-g->min_value)/(g->max_value-g->min_value);
        if(f<0)f=0;
        if(f>1)f=1;
        int bw=(int)((width-12)*f+0.5);
        if(bw>0)fill(top,x+6,y+24,bw,6,HIGHLIGHT);
    }
}
static void top_dialog(void){
    draw_header(dialog_info?"INI HELP / DETAILS":"ECU SETTINGS / INI PANEL");
    int at=dialog_at(dialog_pick);
    if(at<0){
        int gauges=dialog_gauge_count();
        if(gauges==0){text(top,10,46,"EMPTY INI DIALOG",MUTED,26);return;}
        tiny_fit(top,10,30,dialog_label,HIGHLIGHT,238);
        for(int j=0;j<3;j++){
            int ix=dialog_gauge_page*3+j,ri=ix<gauges?dialog_gauge_at(ix):-1;
            if(ri>=0)draw_ref_gauge(bt_gauge(&def,def.refs[ri].name),8,50+j*43,240);
        }
        tiny_text(top,10,182,"L/R CHANGE REFERENCE GAUGES",MUTED,232);
        return;
    }
    const BtDialogRef*r=&def.refs[at];
    text(top,10,31,dialog_label,HIGHLIGHT,29);
    text(top,10,50,r->label,WHITE,27);
    if(dialog_info){
        if(dialog_help_index!=at){
            dialog_context[0]=0;dialog_help_index=at;dialog_help_page=0;
            if(sd_ready)bt_ini_context_help(bt_sd_cache_path(),r->name,dialog_context,sizeof(dialog_context));
        }
        text(top,10,69,"INI CONTROL IDENTIFIER",MUTED,26);
        text_wrapped(top,10,84,r->name,CYAN,26,2);
        if(dialog_context[0]){
            text(top,10,112,"INI CONTEXT HELP  L/R: PAGE",MUTED,26);
            /* Explicit paging rather than silently truncating paragraphs. */
            const char *q=dialog_context;
            int skip=dialog_help_page*3;
            while(skip-- > 0 && *q){
                int n=0,last=0;while(q[n]&&q[n]!='\n'&&n<26){if(q[n]==' ')last=n;n++;}
                if(n==26&&q[n]&&q[n]!='\n'&&last>0)n=last;
                q+=n;if(*q=='\n'||*q==' ')q++;
            }
            text_wrapped(top,10,128,q,WHITE,26,3);
        }else if(r->detail[0]){
            text(top,10,113,"DETAILS FROM INI",MUTED,25);
            text_wrapped(top,10,128,r->detail,WHITE,26,3);
        }else if(r->kind==5){
            text_wrapped(top,10,113,r->label,WHITE,26,4);
        }else if(r->condition[0]){
            text(top,10,113,"INI CONDITION",MUTED,25);
            text_wrapped(top,10,128,r->condition,WHITE,26,3);
        }else text(top,10,132,"NO ADDITIONAL INI HELP",MUTED,26);
        return;
    }
    if(dialog_help_index!=at){
        dialog_context[0]=0;dialog_help_index=at;dialog_help_page=0;
        if(sd_ready)bt_ini_context_help(bt_sd_cache_path(),r->name,dialog_context,sizeof(dialog_context));
    }
    char b[80];
    if(r->kind==1)snprintf(b,sizeof(b),"NESTED PANEL / %.53s",r->name);
    else if(r->kind==2)snprintf(b,sizeof(b),"INI COMMAND / %.53s",r->name);
    else if(r->kind==3)snprintf(b,sizeof(b),"READ ONLY / %.53s",r->name);
    else if(r->kind==4)snprintf(b,sizeof(b),"LIVE OUTPUT / %.53s",r->name);
    else if(r->kind==6)snprintf(b,sizeof(b),"INI LIVE GRAPH / %.53s",r->name);
    else if(r->kind==5)snprintf(b,sizeof(b),"INI INFORMATION");
    else {
        const BtField*f=bt_field(&def,r->name,false);
        snprintf(b,sizeof(b),"%s / %.53s",f?(f->kind==2?"INI CHOICE":f->kind==1?"ARRAY":"SCALAR"):"FIELD",r->name);
    }
    tiny_fit(top,9,71,b,CYAN,238);
    if(r->kind==4){double v=0;
        if(get_value(r->name,&v)){number(v,b,sizeof(b));text(top,10,89,b,GREEN,20);}
    }
    int gauges=dialog_gauge_count();
    if(gauges>0){
        if(dialog_context[0])tiny_fit(top,10,88,dialog_context,MUTED,236);
        else if(r->detail[0])tiny_fit(top,10,88,r->detail,MUTED,236);
        for(int j=0;j<2;j++){
            int ix=dialog_gauge_page*2+j,ri=ix<gauges?dialog_gauge_at(ix):-1;
            if(ri>=0)draw_ref_gauge(bt_gauge(&def,def.refs[ri].name),8,105+j*36,240);
        }
        char b2[60];snprintf(b2,sizeof(b2),"REFERENCE GAUGES %d-%d / %d   L/R MORE",
            dialog_gauge_page*2+1,dialog_gauge_page*2+2<gauges?dialog_gauge_page*2+2:gauges,gauges);
        tiny_fit(top,9,181,b2,HIGHLIGHT,242);
    }else{
        if(dialog_context[0]){
            tiny_text(top,9,109,"DESCRIPTION FROM ECU INI",MUTED,235);
            text_wrapped(top,10,124,dialog_context,WHITE,26,3);
        }else if(r->detail[0]){
            tiny_text(top,9,109,"DETAIL FROM ECU INI",MUTED,235);
            text_wrapped(top,10,124,r->detail,WHITE,26,3);
        }else if(r->kind==5)text_wrapped(top,10,111,r->label,WHITE,26,3);
        else if(r->condition[0])text_wrapped(top,10,111,r->condition,MUTED,26,3);
        else tiny_text(top,9,113,"NO DESCRIPTION IN ECU INI",MUTED,235);
        if(r->kind==2)tiny_text(top,10,177,"INI COMMAND / NOT EXECUTED",HIGHLIGHT,236);
        else tiny_text(top,10,177,"Y FULL DESCRIPTION / A OPEN",HIGHLIGHT,240);
    }
}
static void top_tune(void){
    if(cal_load.active || (chosen_map>=0&&table.valid)){map_top();return;}
    if(dialog_name[0]){top_dialog();return;}
    draw_header("TUNERSTUDIO TREE");
    int count=category_count();
    if(!count){text(top,12,46,"NO TUNING MENU IN ECU INI",MUTED,33);return;}
    int first=tune_cat>7?tune_cat-7:0;
    for(int i=first;i<count&&i<first+8;i++){
        int at=category_at(i),y=29+(i-first)*18;
        bool selected=i==tune_cat;
        if(selected)round_fill(top,6,y,244,17,3,PANEL);
        nav_icon_mini(top,category_icon(def.menus[at].label),11,y+2);
            menu_text(top,30,y,def.menus[at].label,selected?HIGHLIGHT:WHITE,214);
    }
    if(tune_focus){
        int ix=tune_item_at(list_pick);
        if(ix>=0){
            const BtMenu*m=&def.menus[ix];
            /* Reserve the final line for the selected target, not on top of
             * the eighth large-font category. */
            fill(top,0,176,256,16,BG);
            tiny_fit(top,9,181,m->target[0]?m->target:"INI FOLDER",CYAN,241);
        }
    }
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
static void list_rows(int count){
    if(count<1){text(bottom,10,65,"NO ECU OUTPUT CHANNELS",MUTED,34);return;}
    for(int row=0;row<8&&list_scroll+row<count;row++){
        int ix=list_scroll+row,y=28+row*16;
        bool selected=ix==list_pick;
        if(selected)round_fill(bottom,6,y,244,15,3,PANEL);
        text(bottom,12,y+3,def.channels[ix].name,selected?HIGHLIGHT:WHITE,32);
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
    const BtMap *map=selected_map();if(!map)return;
    const BtField *f=focused_cal_field();
    char b[60];
    bool option=f&&f->kind==2;
    bool scalar=chosen_map==BTS_MAX_MAPS&&table.width==1&&table.height==1;
    tiny_fit(bottom,8,24,map->label,HIGHLIGHT,table.width>1&&table.height>1?150:207);
    if(table.width>1&&table.height>1){
        round_fill(bottom,165,23,46,17,3,PANEL);
        tiny_text(bottom,172,27,map_surface_3d?"2D MAP":"3D MAP",CYAN,42);
    }
    tiny_text(bottom,224,24,is_favorite(chosen_map==BTS_MAX_MAPS?map->z:map->name)?"[*]":"[+]",HIGHLIGHT,28);
    if(option){
        int current=(int)focused_value();
        snprintf(b,sizeof(b),"INI SELECTION / RAW %d",current);
        text(bottom,9,48,b,MUTED,28);
        if(bit_choice_count>0&&current>=0&&current<bit_choice_count&&bit_choice_labels[current][0]){
            round_fill(bottom,9,72,237,30,5,LITE);
            text(bottom,16,81,bit_choice_labels[current],WHITE,24);
            tiny_text(bottom,9,107,"A OPTIONS / X NEXT / Y PREV",CYAN,236);
        }else text(bottom,9,76,"UNKNOWN ECU VALUE / A FOR OPTIONS",RED,26);
    }else if(scalar){
        number(focused_value(),b,sizeof(b));
        text(bottom,9,52,"ECU CALIBRATION VALUE",MUTED,26);
        text(bottom,14,73,b,WHITE,24);
        text(bottom,151,79,f&&f->unit[0]?f->unit:"",MUTED,16);
        if(f&&f->limits_known){
            snprintf(b,sizeof(b),"LIMITS %.4g TO %.4g",f->low,f->high);
            text(bottom,9,105,b,CYAN,27);
        }
    }else{
        snprintf(b,sizeof(b),"CELL %d,%d / %d X %d",cx+1,cy+1,table.width,table.height);
        text(bottom,10,49,b,MUTED,28);
        number(focused_value(),b,sizeof(b));
        text(bottom,12,73,b,WHITE,23);
        if(table.has_x){number(table.xbins[cx],b,sizeof(b));text(bottom,10,102,"X:",MUTED,2);text(bottom,31,102,b,WHITE,13);}
        if(table.has_y){number(table.ybins[cy],b,sizeof(b));text(bottom,135,102,"Y:",MUTED,2);text(bottom,156,102,b,WHITE,12);}
    }
    if(f&&f->power_cycle){
        tiny_text(bottom,9,166,"ECU RESTART REQUIRED AFTER WRITE",HIGHLIGHT,244);
    }
    if(!option){
        static const char *actions[]={"-","+","STEP","TRACE"};
        const BtMap *here=selected_map();
        bool has_series=here&&here->curve&&next_curve_series(chosen_map)>=0;
        for(int i=0;i<4;i++){
            int x=6+i*62;
            round_fill(bottom,x,120,57,26,4,i==1?LITE:PANEL);
            const char *label=(i==3&&has_series)?"SERIES":actions[i];
            tiny_text(bottom,x+(57-(int)strlen(label)*6)/2,128,label,WHITE,56);
        }
    }else{
        round_fill(bottom,9,120,115,26,4,PANEL);
        round_fill(bottom,132,120,115,26,4,PANEL);
        text(bottom,20,128,"< PREVIOUS",WHITE,12);text(bottom,140,128,"NEXT >",WHITE,12);
    }
    if(editor_description[0]){
        tiny_fit(bottom,8,153,editor_description,WHITE,235);
        tiny_text(bottom,235,153,"?",CYAN,12);
    }else if(note[0])tiny_fit(bottom,8,153,note,HIGHLIGHT,241);
    else tiny_text(bottom,8,153,any_map_dirty()?"LOCAL EDIT / NOT WRITTEN":
            editor_readonly?"INI DISPLAY ONLY":"ECU ORIGINAL / UNCHANGED",MUTED,240);
    tiny_text(bottom,8,181,editor_readonly?"READ ONLY  /  B BACK":
         map_surface_3d?"L+DPAD ORBIT / START BURN / R STAR":
         "START BURN / R FAVORITE / B BACK",HIGHLIGHT,246);
}
static void bottom_write_progress(void){
    fill(bottom,0,0,W,H,BG);
    round_fill(bottom,8,29,240,138,7,PANEL);
    tiny_fit(bottom,19,41,"ECU CALIBRATION TRANSACTION",HIGHLIGHT,216);
    tiny_fit(bottom,17,67,bt_write_phase(),WHITE,225);
    int pct=bt_write_percent();
    round_fill(bottom,17,91,220,16,3,EDGE);
    int filled=(pct*214)/100;
    if(filled>0)round_fill(bottom,20,94,filled,10,2,pct>=78?GREEN:HIGHLIGHT);
    char buf[36];snprintf(buf,sizeof(buf),"%d%%",pct);
    tiny_fit(bottom,105,113,buf,HIGHLIGHT,46);
    tiny_fit(bottom,16,139,"NO RETRY IF RESPONSE IS UNCERTAIN",MUTED,225);
    tiny_fit(bottom,8,180,bt_write_can_cancel()?"B: CANCEL BEFORE ECU ARM":
             "ARM ATTEMPTED / DO NOT INTERRUPT",MUTED,245);
}
static void bottom_burn_confirm(void){
    /* Modal confirmation: START merely opens this panel; no ECU write
     * can occur until a separate A-down event on the next input cycle. */
    round_fill(bottom,8,40,240,114,7,PANEL);
    fill(bottom,13,47,230,2,HIGHLIGHT);
    tiny_fit(bottom,22,57,"CONFIRM ECU BURN",HIGHLIGHT,215);
    tiny_fit(bottom,19,81,"WRITE STAGED CALIBRATION TO ECU?",WHITE,218);
    tiny_fit(bottom,19,99,"BENCH ONLY / VERIFIED BACKUP",MUTED,218);
    round_fill(bottom,17,122,221,23,4,LITE);
    tiny_fit(bottom,28,128,"PRESS A TO BURN  /  B CANCEL",HIGHLIGHT,201);
}
static void bottom_bit_picker(void){
    fill(bottom,0,0,W,H,BG);text(bottom,9,8,"INI DROPDOWN / PREDEFINED",HIGHLIGHT,26);
    const BtField *f=focused_cal_field();
    text(bottom,10,31,f?f->name:"ECU OPTIONS",HIGHLIGHT,27);
    for(int row=0;row<8;row++){
        int visible=bit_picker_scroll+row;
        if(visible>=bit_visible_count)break;
        int index=bit_visible_ids[visible],y=49+row*16;
        bool chosen=visible==bit_picker_pick;
        if(chosen)round_fill(bottom,6,y-2,244,16,3,LITE);
        char number[12];snprintf(number,sizeof(number),"%d",index);
        text(bottom,10,y,number,MUTED,4);
        text(bottom,49,y,bit_choice_labels[index],chosen?HIGHLIGHT:WHITE,21);
    }
    tiny_text(bottom,8,181,"A SELECT    B CANCEL",MUTED,242);
}
static void bottom_tune(void){
    if(bt_write_busy()){bottom_write_progress();return;}
    if(cal_load.active){
        text(bottom,9,39,"READING ACTUAL ECU DATA",HIGHLIGHT,28);
        text(bottom,9,68,"THE UI REMAINS RESPONSIVE",WHITE,29);
        text(bottom,9,100,"PLEASE CHECK THE UPPER SCREEN",MUTED,28);
        text(bottom,9,149,"B CANCELS THE READ",HIGHLIGHT,29);
        return;
    }
    /* The active editor takes priority over its parent nested dialog. */
    if(keypad_open){bottom_keypad();return;}
    if(bit_picker_open){bottom_bit_picker();return;}
    if(chosen_map>=0&&table.valid&&editor_help_visible){
        const BtMap *m=selected_map();
        tiny_fit(bottom,9,11,m?m->label:"INI DESCRIPTION",HIGHLIGHT,237);
        text_wrapped(bottom,10,37,editor_description[0]?editor_description:
            "NO CONTEXT HELP IS PROVIDED BY THIS ECU INI.",WHITE,25,8);
        tiny_text(bottom,8,181,"B BACK TO EDITOR",MUTED,225);
        return;
    }
    if(chosen_map>=0&&table.valid){
        if(table.confirm_burn)bottom_burn_confirm();
        else bottom_editor();
        return;
    }
    if(dialog_name[0]){
        tiny_fit(bottom,9,23,dialog_label,HIGHLIGHT,240);
        int total=dialog_count();
        if(!total)text(bottom,10,74,dialog_gauge_count()>0?
            "LIVE REFERENCE GAUGES ABOVE":"NO SUPPORTED FIELDS",MUTED,34);
        for(int row=0;row<8&&dialog_scroll+row<total;row++){
            int ix=dialog_scroll+row,at=dialog_at(ix),y=36+row*18;
            if(at<0)continue;
            bool sel=ix==dialog_pick;
            if(sel)round_fill(bottom,6,y,244,16,3,PANEL);
            const BtDialogRef *r=&def.refs[at];
            menu_text(bottom,12,y+1,r->label,sel?HIGHLIGHT:WHITE,207);
            if(is_favorite(r->name))text(bottom,228,y+3,"*",HIGHLIGHT,2);
            if(r->kind==2)text(bottom,241,y+4,"!",CYAN,1);
            if(r->kind==6)text(bottom,241,y+4,"G",CYAN,1);
            if(r->kind==1)text(bottom,241,y+4,">",CYAN,1);
        }
        tiny_text(bottom,8,182,"A OPEN   X STAR   Y HELP   B BACK",MUTED,245);
        return;
    }
    int count=tune_item_count();
    const char*cat="ECU MENU";
    int index=category_at(tune_cat);if(index>=0)cat=def.menus[index].label;
    menu_text(bottom,9,20,tune_group>=0?def.menus[tune_group].label:cat,HIGHLIGHT,237);
    if(!count){text(bottom,11,75,"NO ECU SETTINGS IN CATEGORY",MUTED,34);return;}
    for(int row=0;row<8&&list_scroll+row<count;row++){
        int idx=tune_item_at(list_scroll+row),y=36+row*18;
        if(idx<0)continue;
        bool selected=list_scroll+row==list_pick;
        if(selected)round_fill(bottom,6,y,244,16,3,PANEL);
        nav_icon_mini(bottom,category_icon(def.menus[idx].label),10,y+1);
        menu_text(bottom,28,y+1,def.menus[idx].label,selected?HIGHLIGHT:WHITE,191);
        if(def.menus[idx].target[0] && is_favorite(def.menus[idx].target))
            text(bottom,224,y+3,"*",HIGHLIGHT,2);
        if(def.menus[idx].group==1)text(bottom,240,y+4,">",CYAN,1);
        else if(map_for_target(def.menus[idx].target)>=0)text(bottom,240,y+4,"M",CYAN,1);
    }
    tiny_text(bottom,8,182,tune_focus?"A OPEN   X STAR   B BACK":"UP/DOWN CATEGORY   A OPEN",MUTED,246);
}
/* 3DS layout parity: workspace launcher is a vertical icon-left list.
 * The upper screen remains exclusively the live dashboard. */
static void bottom_favorites(void){
    tiny_text(bottom,9,23,"STARRED TUNING SETTINGS",HIGHLIGHT,236);
    if(!favorite_count)text(bottom,9,79,"PRESS X ON A TUNING SETTING",MUTED,27);
    for(int r=0;r<8&&list_scroll+r<favorite_count;r++){
        int i=list_scroll+r,y=36+r*18;
        if(i==list_pick)round_fill(bottom,6,y,244,16,3,PANEL);
        tiny_fit(bottom,10,y+3,favorites[i].label,i==list_pick?HIGHLIGHT:WHITE,208);
        text(bottom,228,y+3,"*",HIGHLIGHT,2);
    }
    tiny_text(bottom,8,182,"A OPEN   X REMOVE   B HOME",MUTED,246);
}
static void bottom_home(void){
    text(bottom,8,5,"BRICK TUNER / WORKSPACES",HIGHLIGHT,26);
    for(int i=0;i<4;i++){
        int y=27+i*32;
        bool selected=home_pick==i;
        round_fill(bottom,6,y,244,29,4,selected?LITE:PANEL);
        if(selected)fill(bottom,7,y+6,3,17,HIGHLIGHT);
        nav_icon_mini(bottom,home_icons[i],14,y+7);
        text(bottom,34,y+9,home_labels[i],selected?WHITE:MUTED,23);
        text(bottom,237,y+9,">",selected?HIGHLIGHT:EDGE,1);
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
    if(graph_from_ini && active_ini_graph>=0 && active_ini_graph<def.graph_count){
        const BtIniGraph *g=&def.graphs[active_ini_graph];
        tiny_fit(bottom,8,26,g->title,HIGHLIGHT,238);
        for(int i=0;i<8&&graph_list_start+i<g->line_count;i++){
            int offset=g->first_line+graph_list_start+i;
            if(offset>=def.graph_line_count)break;
            const char *ch=def.graph_lines[offset].channel;
            int y=38+i*17;
            bool selected=graph_list_start+i==active_ini_graph_line;
            if(selected)round_fill(bottom,5,y,246,16,3,PANEL);
            tiny_fit(bottom,10,y+3,ch,selected?HIGHLIGHT:WHITE,172);
            double v=0;char b[32];
            if(get_value(ch,&v)){number(v,b,sizeof(b));tiny_fit(bottom,189,y+3,b,GREEN,61);}
            else tiny_text(bottom,189,y+3,"--",MUTED,40);
        }
        tiny_text(bottom,8,180,"X NEXT CH  /  Y STATS  /  B BACK",MUTED,246);
    }else{
        tiny_text(bottom,10,30,"GRAPH CHANNELS",HIGHLIGHT,225);
        for(int i=0;i<SLOTS;i++){
            int y=50+i*42;
            round_fill(bottom,7,y,241,35,4,i==active_slot?LITE:PANEL);
            tiny_fit(bottom,15,y+5,channel_names[i],i==active_slot?HIGHLIGHT:WHITE,225);
            double v=0;char b[32];
            if(get_value(channel_names[i],&v)){
                number(v,b,sizeof(b));tiny_fit(bottom,15,y+21,b,GREEN,180);
            }
        }
        tiny_text(bottom,9,181,"Y STATS  /  L-R CHANNELS  /  B BACK",MUTED,246);
    }
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
    else if(page==P_LIVE)top_channels();
    else if(page==P_GRAPH)top_graph();
    else if(page==P_SOUNDS)top_sounds();
    else if(page==P_FAV && (cal_load.active||(chosen_map>=0&&table.valid)))map_top();
    else if(page==P_FAV){
        draw_header("FAVORITE ECU SETTINGS");
        text(top,11,44,"QUICK ACCESS TO INI SETTINGS",HIGHLIGHT,27);
        text(top,11,75,"SAME EDITORS AS THE TUNING TREE",WHITE,27);
        text(top,11,104,"X: REMOVE SELECTED FAVORITE",MUTED,27);
        text(top,11,133,"FAVORITES SAVED ON SD PER ECU",MUTED,27);
    }
    else top_link();
    fill(bottom,0,0,W,H,BG);fill(bottom,0,0,W,23,PANEL);fill(bottom,0,22,W,1,EDGE);
    if(page!=P_HOME){text(bottom,9,6,"< HOME",HIGHLIGHT,6);text(bottom,94,6,tabs[page],WHITE,18);}
    if(idle_dashboard)bottom_idle_gauges();
    else if(page==P_HOME)bottom_home();
    else if(page==P_TUNE)bottom_tune();
    else if(page==P_LIVE)list_rows(def.channel_count);
    else if(page==P_GRAPH)bottom_graph();
    else if(page==P_SOUNDS)bottom_sounds();
    else if(page==P_FAV){if(cal_load.active||(chosen_map>=0&&table.valid)||keypad_open)bottom_tune();else bottom_favorites();}
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
        loading_ini=false;
        if(bt_write_busy()){
            bt_write_link_lost();
            /* Pre-arm link loss ends the transaction immediately; do not
             * leave the user looking at a stale "BURN IN PROGRESS" notice. */
            if(!bt_write_busy()){
                snprintf(note,sizeof(note),"%s",bt_write_message());
                write_baseline=write_edited=NULL;write_length=0;
            }
        }else{bt_async_cancel();bt_async_forget_connection();}
        set_compat();invalidate();
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
                bt_ini_reset(&def);reset_ref_index();build_map_index();invalidate();
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
        reset_ref_index();build_map_index();set_compat();ini_retry_frame=0;
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
    const BtMap *active=(chosen_map>=0&&table.valid)?selected_map():NULL;
    if(active){
        for(int i=0;i<2;i++)if(trace_formula_valid[i])
            length=expression_live_length(trace_formula[i],length);
    }
    if(page==P_GRAPH&&graph_from_ini&&active_ini_graph>=0&&active_ini_graph<def.graph_count){
        const BtIniGraph *g=&def.graphs[active_ini_graph];
        for(int j=0;j<g->line_count;j++){
            const char *ch=def.graph_lines[g->first_line+j].channel;
            const BtField *f=bt_field(&def,ch,true);
            if(f&&f->scaled){int end=f->offset+bt_type_size(f->type);
                if(end>length&&end<=BTS_LIVE_CAP)length=end;
            }
        }
        for(int i=0;i<SLOTS;i++)if(graph_formula_valid[i])
            length=expression_live_length(graph_formula[i],length);
        for(int i=0;i<8;i++)if(graph_list_valid[i])
            length=expression_live_length(graph_list_formulas[i],length);
    }
    /* TunerStudio dialog reference gauges require live bytes even when
     * the user has not selected those channels in the generic View tab. */
    if((page==P_TUNE||page==P_FAV)&&dialog_name[0]){
        int count=dialog_gauge_count();
        for(int i=0;i<count;i++){
            int ref=dialog_gauge_at(i);
            if(ref<0)continue;
            const BtGauge *g=bt_gauge(&def,def.refs[ref].name);
            if(!g)continue;
            const BtField *f=bt_field(&def,g->channel,true);
            if(f){int end=f->offset+bt_type_size(f->type);
                if(end>length&&end<=BTS_LIVE_CAP)length=end;
            }else if(i<64&&dialog_gauge_expr_valid[i])
                length=expression_live_length(dialog_gauge_expr[i],length);
        }
    }
    /* Preserve a small 30Hz response while navigating; request the larger
     * six-channel capture only when the lower idle dashboard is visible. */
    if(idle_dashboard || (page==P_HOME&&gauge_layout>2))length=bt_idle_required_length(&def,&idle_gauges,length);
    return length;
}
static void service_write(void){
    int r=bt_write_poll(frame);
    if(!r)return;
    if(r>0 && write_baseline && write_edited && write_length>0){
        memcpy(write_baseline,write_edited,(size_t)write_length);
        if(write_focus==0)table.dirty=false;
        else if(write_focus<=2)table.axis_dirty[write_focus-1]=false;
        snprintf(status_msg,sizeof(status_msg),"ECU BURN VERIFIED / READBACK MATCHED");
    }else snprintf(status_msg,sizeof(status_msg),"ECU WRITE FAILED / INSPECT STATUS");
    snprintf(note,sizeof(note),"%s",bt_write_message());
    write_baseline=write_edited=NULL;write_length=0;
    want_status=true;
}
static void service_network(void){
    if(bt_write_busy()){service_write();return;}
    if(cal_load.active){calibration_service();return;}
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
static void favorite_current_editor(void){
    const BtMap *m=selected_map();
    if(!m)return;
    toggle_favorite(chosen_map==BTS_MAX_MAPS?m->z:m->name,m->label);
}
static void open_map(int id){
    if(id<0||(id>=def.map_count&&id!=BTS_MAX_MAPS)||!ini_ok)return;
    const BtMap *m=id==BTS_MAX_MAPS?&scalar_map:&def.maps[id];
    const BtField *z=bt_field(&def,m->z,false);
    /* Some TS curves are generated from defaultValue arrays (no ECU offset).
     * Show them as read-only rather than failing to open, or faking writes. */
    static double ini_virtual_z[BTS_MAP_CELLS];
    int virtual_count=0;
    bool virtual_z=!z&&sd_ready&&bt_ini_virtual_array(bt_sd_cache_path(),m->z,
                                       ini_virtual_z,BTS_MAP_CELLS,&virtual_count);
    int size=z?bt_type_size(z->type):0;
    if(!virtual_z && (!z||size<1||z->page<1||z->page>64||z->width<1||z->height<1||
       z->width*z->height>BTS_MAP_CELLS)){
        snprintf(note,sizeof(note),"INI CURVE NOT ECU-BACKED / NO DECLARED VALUES");return;
    }
    int bytes=virtual_z?0:size*z->width*z->height;
    if(!virtual_z && (bytes<1||bytes>(int)sizeof(table.baseline))){
        snprintf(note,sizeof(note),"INI CALIBRATION EXCEEDS DS RAM BUDGET");return;
    }
    /* Select the workspace immediately. Do not perform blocking socket I/O
     * in a D-pad or touchscreen handler. Only the calibration R worker reads. */
    bt_async_cancel();
    memset(&table,0,sizeof(table));
    memset(&cal_load,0,sizeof(cal_load));
    memset(active_fields,0,sizeof(active_fields));
    memset(active_formats,0,sizeof(active_formats));
    memset(active_field_present,0,sizeof(active_field_present));
    memset(mode_valid,0,sizeof(mode_valid));
    memset(trace_formula_valid,0,sizeof(trace_formula_valid));
    virtual_z_map=virtual_z;
    table.width=virtual_z?virtual_count:z->width;
    table.height=virtual_z?1:z->height;
    table.raw_length=bytes;
    if(virtual_z)memcpy(table.cells,ini_virtual_z,(size_t)virtual_count*sizeof(double));
    editor_description[0]=0;editor_help_visible=false;
    if(sd_ready){
        if(!(z&&bt_ini_context_help(bt_sd_cache_path(),z->name,editor_description,sizeof(editor_description)))
           &&m->help_topic[0])
            bt_ini_topic_help(bt_sd_cache_path(),m->help_topic,editor_description,sizeof(editor_description));
    }
    for(int j=0;j<3;j++){
        const char *field_name=j==0?m->z:j==1?m->x:m->y;
        const BtField *source=bt_field(&def,field_name,false);
        if(!source)continue;
        active_fields[j]=*source;
        active_field_present[j]=true;
        if(sd_ready)bt_ini_field_format(bt_sd_cache_path(),field_name,&active_formats[j]);
    }
    for(int j=0;j<2;j++){
        const char *ch=j==0?m->xchan:m->ychan;
        if(sd_ready && ch&&*ch && !bt_field(&def,ch,true)){
            trace_formula_valid[j]=bt_ini_rhs(bt_sd_cache_path(),"OutputChannels",ch,
                 trace_formula[j],sizeof(trace_formula[j]));
        }
    }
    editor_readonly=editor_readonly||virtual_z||(z&&z->read_only);
    if(virtual_z){
        memset(&active_fields[0],0,sizeof(active_fields[0]));
        snprintf(active_fields[0].name,sizeof(active_fields[0].name),"%.63s",m->z);
        active_fields[0].kind=1;active_fields[0].width=virtual_count;
        active_fields[0].height=1;active_fields[0].type=BT_F64;
        active_fields[0].scale=1;active_fields[0].scaled=true;
        active_fields[0].read_only=true;active_field_present[0]=true;
    }
    chosen_map=id;cx=cy=0;map_focus=0;abandon_confirm=false;keypad_open=false;bit_picker_open=false;
    map_surface_3d=table.width>1&&table.height>1;
    cal_load.active=true;cal_load.stage=-3;cal_load.total=0;cal_load.map_id=id;
    snprintf(status_msg,sizeof(status_msg),"READING ECU CALIBRATION");
    snprintf(note,sizeof(note),"READING %s FROM ECU",m->label);
}
static const BtField *cal_stage_field(void){
    if(!selected_map())return NULL;
    if(cal_load.stage<0||cal_load.stage>2||!active_field_present[cal_load.stage])return NULL;
    return &active_fields[cal_load.stage];
}
static void cal_load_fail(const char *why){
    cal_load.active=false;bt_async_cancel();
    chosen_map=-1;table.valid=false;
    snprintf(status_msg,sizeof(status_msg),"CALIBRATION READ FAILED");
    snprintf(note,sizeof(note),"READ FAILED: %.76s",why?why:"NO ECU DATA");
}
static void cal_stage_complete(void){
    const BtField *f=cal_stage_field();
    if(cal_load.stage==0){
        if(!f){cal_load_fail("INI FIELD DISAPPEARED");return;}
        memcpy(table.edited,table.baseline,(size_t)table.raw_length);
        BtField relative=*f;relative.offset=0;
        /* A conditional/macro INI formula cannot safely be invented. Such
         * fields are viewable in raw units but editing stays disabled. */
        if(!relative.scaled){relative.scaled=true;relative.scale=1;relative.translate=0;}
        for(int i=0;i<table.width*table.height;i++){
            if(!bt_field_value(&def,&relative,table.baseline,table.raw_length,i,&table.cells[i])){
                cal_load_fail("ECU BYTES OR INI REPRESENTATION INVALID");return;
            }
        }
    }else if(f){
        BtField relative=*f;relative.offset=0;
        if(!relative.scaled){relative.scaled=true;relative.scale=1;relative.translate=0;}
        int count=f->width*f->height;
        double *axis=cal_load.stage==1?table.xbins:table.ybins;
        bool valid=count>0&&count<=64;
        for(int i=0;i<count&&valid;i++)
            valid=bt_field_value(&def,&relative,cal_load.axis_raw,cal_load.total,i,&axis[i]);
        if(valid){
            int axis_index=cal_load.stage-1;
            memcpy(table.axis_baseline[axis_index],cal_load.axis_raw,(size_t)cal_load.total);
            memcpy(table.axis_edited[axis_index],cal_load.axis_raw,(size_t)cal_load.total);
            table.axis_length[axis_index]=cal_load.total;
        }
        if(cal_load.stage==1){table.has_x=valid;table.x_count=valid?count:0;}
        else {table.has_y=valid;table.y_count=valid?count:0;}
    }
    cal_load.stage++;
    cal_load.offset=0;cal_load.total=0;
    if(cal_load.stage>=3){
        cal_load.active=false;table.valid=true;
        const BtField *z=active_field_present[0]?&active_fields[0]:NULL;
        snprintf(status_msg,sizeof(status_msg),"ECU CALIBRATION READ COMPLETE");
        snprintf(note,sizeof(note),"%s",z&&z->scaled?
            "ECU VALUES LOADED / EDIT LOCALLY":"RAW VIEW ONLY / INI FORMULA NOT DECODED");
    }
}
static void cal_skip_optional_axis(const char *cause){
    /* A broken/unavailable X/Y-axis read must not lock out a perfectly valid
     * Z table; retain its calibrated ECU data and disable only that axis. */
    if(cal_load.stage==0){cal_load_fail(cause);return;}
    bt_async_cancel();
    cal_load.stage++;cal_load.offset=0;cal_load.total=0;
    snprintf(note,sizeof(note),"AXIS READ SKIPPED: %.64s",cause?cause:"UNAVAILABLE");
    if(cal_load.stage>=3){
        cal_load.active=false;table.valid=true;
        snprintf(status_msg,sizeof(status_msg),"ECU MAP READY / AXIS UNAVAILABLE");
    }
}
static bool read_mode_block(int at,const uint8_t *bytes,int length){
    bool found=false;
    for(int i=0;i<6;i++){
        const BtField*source=bt_field(&def,mode_names[i],false);
        if(!source||source->page!=1||source->offset<at||
           source->offset+bt_type_size(source->type)>at+length)continue;
        BtField relative=*source;relative.offset-=at;
        double v=0;
        if(bt_field_value(&def,&relative,bytes,length,0,&v)){
            mode_values[i]=v;mode_valid[i]=true;found=true;
        }
    }
    return found;
}
static void calibration_service(void){
    if(!cal_load.active)return;
    if(!wifi_up||!ecu_up){cal_load_fail("ECU NETWORK NOT READY");return;}
    if(cal_load.stage<0){
        static const int addresses[]={12,456,1609};
        static const int lengths[]={4,1,28};
        int stage=cal_load.stage+3;
        if(bt_async_busy()){
            int result=0;
            for(int n=0;n<4 && result==0;n++)result=bt_async_poll(frame);
            if(result==0)return;
            if(result>0){
                uint8_t raw[64];
                if(bt_async_cal_read_result(raw,lengths[stage]))
                    read_mode_block(addresses[stage],raw,lengths[stage]);
            }
            bt_async_cancel();
            cal_load.stage++;
            if(cal_load.stage==0){resolve_active_formats();cal_load.total=0;}
            return;
        }
        int id=def.page_ids[0];
        if(id<0||!bt_async_begin_cal_read(id,addresses[stage],lengths[stage],frame)){
            /* Read failure never turns an unknown mode into an assumed one. */
            cal_load.stage++;
            if(cal_load.stage==0){resolve_active_formats();cal_load.total=0;}
        }
        return;
    }
    if(bt_async_busy()){
        int result=0;
        for(int n=0;n<4&&result==0;n++)result=bt_async_poll(frame);
        if(result==0)return;
        if(result<0){
            char error[92];
            const char *reason=bt_async_reject_body();
            if(reason[0])snprintf(error,sizeof(error),"HTTP %d: %.65s",bt_async_http_status(),reason);
            else snprintf(error,sizeof(error),"%.90s",bt_async_error());
            if(cal_load.stage>0)cal_skip_optional_axis(error);
            else cal_load_fail(error);
            return;
        }
        uint8_t bytes[256];
        if(!bt_async_cal_read_result(bytes,cal_load.chunk)){
            if(cal_load.stage>0)cal_skip_optional_axis("BRICKOS R RESPONSE INVALID / ECU STATUS");
            else cal_load_fail("BRICKOS R RESPONSE INVALID / ECU STATUS");
            return;
        }
        uint8_t *destination=cal_load.stage==0?table.baseline:cal_load.axis_raw;
        memcpy(destination+cal_load.offset,bytes,(size_t)cal_load.chunk);
        cal_load.offset+=cal_load.chunk;
        bt_async_cancel();
        if(cal_load.offset>=cal_load.total)cal_stage_complete();
        return;
    }
    /* Z is absent from ECU RAM for a declared INI virtual curve. */
    if(cal_load.stage==0&&virtual_z_map)cal_load.stage=1;
    /* Stage 0 is mandatory Z for real ECU maps; X/Y can be addressable or
     * read-only [ConstantsExtensions] arrays with literal INI defaults. */
    while(cal_load.active&&cal_load.total==0){
        if(cal_load.stage>0&&!active_field_present[cal_load.stage]&&sd_ready){
            const BtMap *map=selected_map();
            const char *axis=map?(cal_load.stage==1?map->x:map->y):"";
            int required=cal_load.stage==1?table.width:table.height;
            int count=0;
            double *bins=cal_load.stage==1?table.xbins:table.ybins;
            if(axis&&*axis&&bt_ini_virtual_array(bt_sd_cache_path(),axis,bins,64,&count)
               &&count>=required){
                if(cal_load.stage==1){table.has_x=true;table.x_count=count;}
                else{table.has_y=true;table.y_count=count;}
                cal_load.stage++;continue;
            }
        }
        const BtField *f=cal_stage_field();
        int count=f?f->width*f->height:0;
        int size=f?bt_type_size(f->type):0;
        const BtField *z=active_field_present[0]?&active_fields[0]:NULL;
        bool optional=cal_load.stage>0;
        int required=cal_load.stage==1?table.width:table.height;
        bool usable=f&&size>0&&f->page>=1&&f->page<=64&&count>0&&
            (!optional || (count>=required&&count<=64&&size*count<=512));
        if(!usable){
            if(optional){cal_load.stage++;if(cal_load.stage>=3){
                cal_load.active=false;table.valid=true;
                snprintf(note,sizeof(note),"%s",z&&z->scaled?
                   "ECU VALUES LOADED / EDIT LOCALLY":"RAW VIEW / INI FORMULA UNRESOLVED");
                snprintf(status_msg,sizeof(status_msg),"ECU CALIBRATION READY");
            }continue;}
            cal_load_fail("INVALID ECU Z CALIBRATION DEFINITION");return;
        }
        cal_load.total=count*size;
        cal_load.offset=0;
    }
    if(!cal_load.active)return;
    const BtField *f=cal_stage_field();
    int id=f->page<=def.page_count?def.page_ids[f->page-1]:(f->page-1)<<8;
    if(id<0||id>65535||f->offset<0||f->offset+cal_load.total>65536){
        if(cal_load.stage>0){cal_load.stage++;cal_load.total=0;return;}
        cal_load_fail("ECU PAGE ID OR OFFSET INVALID");return;
    }
    cal_load.chunk=cal_load.total-cal_load.offset;
    if(cal_load.chunk>256)cal_load.chunk=256;
    if(!bt_async_begin_cal_read(id,f->offset+cal_load.offset,cal_load.chunk,frame)){
        char error[92];snprintf(error,sizeof(error),"%.90s",bt_async_error());
        if(cal_load.stage>0)cal_skip_optional_axis(error[0]?error:"NO AXIS DATA");
        else cal_load_fail(error[0]?error:"COULD NOT START ECU R REQUEST");
    }
}
static const BtField *focused_cal_field(void){
    const BtMap *m=selected_map();
    if(!m)return NULL;
    if(map_focus==1&&table.has_x)return active_field_present[1]?&active_fields[1]:NULL;
    if(map_focus==2&&table.has_y)return active_field_present[2]?&active_fields[2]:NULL;
    return active_field_present[0]?&active_fields[0]:NULL;
}
static int focused_cell(void){return map_focus==1?cx:map_focus==2?cy:cy*table.width+cx;}
static double focused_value(void){
    if(map_focus==1&&table.has_x)return table.xbins[cx];
    if(map_focus==2&&table.has_y)return table.ybins[cy];
    return table.cells[cy*table.width+cx];
}
static bool focused_buffers(const BtField **field,uint8_t **baseline,
                            uint8_t **edited,int *length,int *cell){
    if(!table.valid||chosen_map<0)return false;
    const BtField *f=focused_cal_field();
    if(!f)return false;
    if(map_focus){
        int axis=map_focus-1;
        if(!table.axis_length[axis]){
            snprintf(note,sizeof(note),"INI VIRTUAL AXIS / READ ONLY");return false;
        }
        *baseline=table.axis_baseline[axis];*edited=table.axis_edited[axis];
        *length=table.axis_length[axis];
    }else{
        *baseline=table.baseline;*edited=table.edited;*length=table.raw_length;
    }
    *field=f;*cell=focused_cell();return true;
}
static bool any_map_dirty(void){
    return table.dirty||table.axis_dirty[0]||table.axis_dirty[1];
}
/* Staged axes, table and scalars have the SAME write gates but independent
 * EEPROM/flash byte ranges. Each committed field is verified on its own. */
/* All keypad and +/- edits pass through the same INI-scaled encoder.
 * This only changes the local staged buffer, never ECU memory. */
static bool set_map_cell(double value){
    if(chosen_map<0||!table.valid||!ini_ok||!isfinite(value)||editor_readonly)return false;
    const BtMap *m=selected_map();
    if(m && ((map_focus==1&&m->x_readonly)||(map_focus==2&&m->y_readonly))){
        snprintf(note,sizeof(note),"ECU INI MARKS AXIS READ ONLY");return false;
    }
    const BtField *z=focused_cal_field();
    if(!z||!z->scaled){snprintf(note,sizeof(note),"INI SCALING UNAVAILABLE / READ ONLY");return false;}
    if(map_focus==0&&z->kind==2&&bit_choice_count==0){
        snprintf(note,sizeof(note),"INI OPTIONS NOT AVAILABLE / READ ONLY");return false;
    }
    if(map_focus==0&&z->kind==2&&bit_choice_count>0){
        int choice=(int)value;
        if(value!=floor(value)||choice<0||choice>=bit_choice_count||!bit_choice_labels[choice][0]){
            snprintf(note,sizeof(note),"CHOICE NOT DEFINED BY ECU INI");return false;
        }
    }
    const BtField *field=NULL;
    uint8_t *baseline=NULL,*edited=NULL;
    int length=0,cell=0;
    if(!focused_buffers(&field,&baseline,&edited,&length,&cell))return false;
    int size=bt_type_size(field->type);
    if(size<1||size>8||cell<0||cell*size+size>length)return false;
    BtField rel=*field;rel.offset=0;
    uint8_t previous[8];memcpy(previous,edited+cell*size,(size_t)size);
    if(!bt_encode_value(&def,&rel,edited,length,cell,value)||
       !bt_field_value(&def,&rel,edited,length,cell,&value)){
        memcpy(edited+cell*size,previous,(size_t)size);
        snprintf(note,sizeof(note),"VALUE OUTSIDE ECU INI LIMITS");return false;
    }
    if(map_focus==1)table.xbins[cx]=value;
    else if(map_focus==2)table.ybins[cy]=value;
    else table.cells[cell]=value;
    if(map_focus==0)table.dirty=memcmp(edited,baseline,(size_t)length)!=0;
    else table.axis_dirty[map_focus-1]=memcmp(edited,baseline,(size_t)length)!=0;
    table.confirm_burn=false;abandon_confirm=false;note[0]=0;
    return true;
}
static void change_map_cell(int direction){
    if(chosen_map<0||!table.valid||editor_readonly)return;
    const BtField *z=focused_cal_field();if(!z)return;
    double delta=z->kind==2?1.0:steps[step_index];
    if(fabs(z->scale)>delta)delta=fabs(z->scale);
    double next=focused_value()+delta*direction;
    if(z->kind==2&&bit_choice_count>0){
        for(int attempt=0;attempt<bit_choice_count;attempt++){
            int index=(int)next;
            if(index>=0&&index<bit_choice_count&&bit_choice_labels[index][0])break;
            next+=direction;
        }
    }
    (void)set_map_cell(next);
}
static void bit_picker_show(void){
    if(chosen_map<0||!table.valid||editor_readonly||!bit_choice_count)return;
    const BtField *z=focused_cal_field();
    if(!z||z->kind!=2||map_focus!=0)return;
    int current=(int)focused_value();
    /* The UI indexes only defined options; each item still writes its
     * original numeric INI value, never an artificially compacted ID. */
    bit_picker_pick=0;
    for(int i=0;i<bit_visible_count;i++)if(bit_visible_ids[i]==current){bit_picker_pick=i;break;}
    bit_picker_scroll=bit_picker_pick>3?bit_picker_pick-3:0;
    bit_picker_open=bit_visible_count>0;
}
static void keypad_begin(void){
    if(!table.valid||chosen_map<0||editor_readonly)return;
    const BtField *z=focused_cal_field();
    if(z&&z->kind==2){bit_picker_show();return;}
    if(!z||!z->scaled){snprintf(note,sizeof(note),"INI EXPRESSION NOT DECODABLE / READ ONLY");return;}
    snprintf(keypad_input,sizeof(keypad_input),"%.8g",focused_value());
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
    if(editor_readonly){snprintf(note,sizeof(note),"INI DISPLAY ONLY / WRITE BLOCKED");return;}
    const BtMap *map=selected_map();
    if(map && ((map_focus==1&&map->x_readonly)||(map_focus==2&&map->y_readonly))){
        snprintf(note,sizeof(note),"ECU INI AXIS READ ONLY / WRITE BLOCKED");return;
    }
    if(!any_map_dirty()||!selected_map()||!ini_ok){
        snprintf(note,sizeof(note),"NO PENDING CALIBRATION EDITS");return;
    }
    const BtField *field=NULL;
    uint8_t *baseline=NULL,*edited=NULL;
    int length=0,cell=0;
    if(!focused_buffers(&field,&baseline,&edited,&length,&cell)||!field||!field->scaled){
        snprintf(note,sizeof(note),"INI SCALE UNKNOWN / ECU WRITE BLOCKED");return;
    }
    if(!memcmp(baseline,edited,(size_t)length)){
        snprintf(note,sizeof(note),"SELECT YOUR EDITED Z/X/Y FIELD FIRST");return;
    }
    if(!table.confirm_burn){table.confirm_burn=true;
        snprintf(note,sizeof(note),"PRESS A TO BURN / B TO CANCEL");return;}
    table.confirm_burn=false;
    bt_async_cancel();bt_async_forget_connection();
    if(!bt_write_begin(&def,field,baseline,edited,length,frame)){
        snprintf(note,sizeof(note),"BURN NOT STARTED / CHECK INI AND ECU STATUS");
        return;
    }
    write_baseline=baseline;write_edited=edited;
    write_length=length;write_focus=map_focus;
    snprintf(note,sizeof(note),"ECU BURN IN PROGRESS");
    snprintf(status_msg,sizeof(status_msg),"ECU SAFE WRITE / ASYNC PROGRESS");
}
static void goto_page(int next){
    if(next<0||next>=P_COUNT)return;
    page=next;chosen_map=-1;table.valid=false;table.confirm_burn=false;map_focus=0;bit_picker_open=false;bit_choice_count=bit_visible_count=0;list_pick=list_scroll=0;editor_readonly=false;
    keypad_open=false;
    activity_frame=frame;idle_dashboard=false;
    dialog_name[0]=0;note[0]=0;dialog_depth=0;
    tune_focus=0;tune_group=-1;
}
static void navigate(int direction){
    int count=page==P_LIVE?def.channel_count:page==P_FAV?favorite_count:tune_item_count();
    if(count<=0)return;
    list_pick+=direction;if(list_pick<0)list_pick=0;if(list_pick>=count)list_pick=count-1;
    if(list_pick<list_scroll)list_scroll=list_pick;
    int visible=8;
    if(list_pick>=list_scroll+visible)list_scroll=list_pick-visible+1;
}
static bool is_ini_view_target(const char *target,const char *label){
    if(!target||!label)return false;
    return !strcasecmp(target,"std_realtime")||
           !strcasecmp(target,"std_dashboard")||
           !strcasecmp(target,"std_outputChannels")||
           !strcasecmp(target,"std_outputChannel")||
           !strcasecmp(target,"std_realtimeDisplay")||
           !strcasecmp(target,"std_runtime")||
           !strcasecmp(target,"std_liveGraph");
}
static void open_target(const char *target,const char *label,bool from_view){
    if(!target||!*target)return;
    int map=map_for_target(target);
    if(map>=0){editor_readonly=false;open_map(map);return;}
    for(int i=owner_first(target);i>=0;i=ref_chain[i])
        if(!strcasecmp(target,def.refs[i].owner)){
            open_dialog(target,label);note[0]=0;return;
        }
    if(open_ini_field(target,label,false))return;
    if(is_ini_view_target(target,label) && from_view){
        graph_from_ini=false;active_ini_graph=-1;
        view_from_tree=true;goto_page(P_LIVE);return;
    }
    snprintf(note,sizeof(note),"INI TARGET %.39s / UNSUPPORTED",target);
}
static void open_tune_item(void){
    int id=tune_item_at(list_pick);if(id<0)return;
    const BtMenu*m=&def.menus[id];
    bts_sfx_play(bts_sound_for_entry(m->label,m->target),sd_ready);
    if(m->group==1){tune_group=id;list_pick=list_scroll=0;return;}
    int cat=category_at(tune_cat);
    bool from_view=cat>=0&&!strcasecmp(def.menus[cat].label,"View");
    open_target(m->target,m->label,from_view);
}
static void handle_buttons(uint32_t down,uint32_t held,touchPosition*tp){
    if(bt_write_busy()){
        if(down&KEY_B && bt_write_cancel()){
            snprintf(note,sizeof(note),"ECU BURN CANCELLED BEFORE ARM");
            write_baseline=write_edited=NULL;write_length=0;
        }
        /* No navigation or value editing during a protected transaction;
         * the socket still progresses on every VBlank. */
        return;
    }
    if(cal_load.active){
        if(down&KEY_B){cal_load.active=false;bt_async_cancel();chosen_map=-1;
            snprintf(note,sizeof(note),"ECU READ CANCELLED");}
        return;
    }
    if(bit_picker_open){
        if(down&KEY_B){bit_picker_open=false;return;}
        if(down&KEY_UP && bit_picker_pick>0)bit_picker_pick--;
        if(down&KEY_DOWN && bit_picker_pick+1<bit_visible_count)bit_picker_pick++;
        if(bit_picker_pick<bit_picker_scroll)bit_picker_scroll=bit_picker_pick;
        if(bit_picker_pick>=bit_picker_scroll+8)bit_picker_scroll=bit_picker_pick-7;
        if((down&KEY_TOUCH)&&tp->py>=49&&tp->py<177){
            int next=bit_picker_scroll+(tp->py-49)/16;
            if(next>=0&&next<bit_visible_count){
                bit_picker_pick=next;
                if(set_map_cell((double)bit_visible_ids[next]))bit_picker_open=false;
            }
        }
        if(down&KEY_A&&bit_picker_pick>=0&&bit_picker_pick<bit_visible_count){
            if(set_map_cell((double)bit_visible_ids[bit_picker_pick]))bit_picker_open=false;
        }
        return;
    }
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
        if(table.confirm_burn){
            if(down&KEY_A)commit_map();
            else if(down&KEY_B){table.confirm_burn=false;note[0]=0;}
            return;
        }
        if(editor_help_visible){if(down&KEY_B)editor_help_visible=false;return;}
        if((down&KEY_TOUCH)&&tp->py>=148&&tp->py<174&&editor_description[0]){
            editor_help_visible=true;return;
        }
        bool orbit=map_surface_3d&&((held|down)&KEY_L);
        if(orbit){
            if((down|held)&KEY_LEFT)map_orbit-=0.08;
            if((down|held)&KEY_RIGHT)map_orbit+=0.08;
            if((down|held)&KEY_UP)map_elevation+=0.05;
            if((down|held)&KEY_DOWN)map_elevation-=0.05;
            if(map_elevation<0.26)map_elevation=0.26;
            if(map_elevation>1.30)map_elevation=1.30;
            if(map_orbit>6.283185307)map_orbit-=6.283185307;
            if(map_orbit<0)map_orbit+=6.283185307;
        }else{
            if((down|held)&KEY_LEFT&&cx>0)cx--;
            if((down|held)&KEY_RIGHT&&cx+1<table.width)cx++;
            if((down|held)&KEY_UP&&cy>0)cy--;
            if((down|held)&KEY_DOWN&&cy+1<table.height)cy++;
        }
        if(down&KEY_START){commit_map();return;}
        if(down&KEY_R)favorite_current_editor();
        if((down&KEY_TOUCH)&&tp->py>=24&&tp->py<46){
            if(tp->px>=215)favorite_current_editor();
            else if(tp->px>=163&&table.width>1&&table.height>1)
                map_surface_3d=!map_surface_3d;
        }
        if(down&KEY_X)change_map_cell(1);
        if(down&KEY_Y)change_map_cell(-1);
        if(down&KEY_A)keypad_begin();
        if((down&KEY_L)&&!orbit){step_index=(step_index+1)%4;table.confirm_burn=false;}
        if(down&KEY_SELECT){
            map_focus=map_focus==0&&table.has_x?1:
                map_focus<=1&&table.has_y?2:0;
            table.confirm_burn=false;
        }
        if((down&KEY_TOUCH)&&tp->py>=120&&tp->py<146&&tp->px>=6&&tp->px<254){
            int action=(tp->px-6)/62;
            if(action==0)change_map_cell(-1);
            else if(action==1)change_map_cell(1);
            else if(action==2){step_index=(step_index+1)%4;table.confirm_burn=false;}
            else if(action==3){
                int next=next_curve_series(chosen_map);
                if(next>=0){
                    if(any_map_dirty())snprintf(note,sizeof(note),"SAVE OR DISCARD EDITS BEFORE NEXT SERIES");
                    else open_map(next);
                }else{
                    map_live_trace(selected_map());
                    if(trace_valid){cx=trace_x;cy=trace_y;table.confirm_burn=false;}
                }
            }
        }
        /* R stars a setting; START opens the burn confirmation modal. */
        if(down&KEY_B){
            if(any_map_dirty()&&!abandon_confirm){abandon_confirm=true;table.confirm_burn=false;
                snprintf(note,sizeof(note),"B AGAIN TO DISCARD UNSAVED EDITS");return;}
            chosen_map=-1;table.valid=false;note[0]=0;abandon_confirm=false;
        }
        return;
    }
    if((down&KEY_SELECT)&&page!=P_HOME){goto_page(P_HOME);return;}
    if((down&KEY_TOUCH)&&page!=P_HOME&&tp->py<23){goto_page(P_HOME);return;}
    if(page==P_HOME){
        if((down&KEY_UP)&&home_pick>0)home_pick--;
        if((down&KEY_DOWN)&&home_pick<3)home_pick++;
        if(down&KEY_L){gauge_layout=gauge_layout==2?4:gauge_layout-1;if(sd_ready)bt_sd_layout_save(gauge_layout);}
        if(down&KEY_R){gauge_layout=gauge_layout==4?2:gauge_layout+1;if(sd_ready)bt_sd_layout_save(gauge_layout);}
        if(down&KEY_A)goto_page(home_pages[home_pick]);
        if((down&KEY_TOUCH)&&tp->py>=27&&tp->py<155){
            int row=(tp->py-27)/32;
            if(row>=0&&row<4){home_pick=row;goto_page(home_pages[home_pick]);}
        }
    }else if(page==P_TUNE){
        if(dialog_name[0]){
            int n=dialog_count();
            if(down&KEY_B){
                if(dialog_depth>0){
                    dialog_depth--;
                    memcpy(dialog_name,dialog_owner_stack[dialog_depth],sizeof(dialog_name));
                    memcpy(dialog_label,dialog_label_stack[dialog_depth],sizeof(dialog_label));
                    dialog_pick=dialog_scroll=0;dialog_info=false;dialog_gauge_page=0;
                    rebuild_dialog_entries();
                    rebuild_dialog_gauges();
                }else dialog_name[0]=0;
                return;
            }
            if(down&KEY_Y){dialog_info=!dialog_info;dialog_help_index=-1;dialog_help_page=0;}
            if(dialog_info && (down&KEY_L) && dialog_help_page>0)dialog_help_page--;
            if(dialog_info && (down&KEY_R) && dialog_help_page<30)dialog_help_page++;
            if(!dialog_info){
                int gauges=dialog_gauge_count(),per=dialog_count()>0?2:3;
                int pages=(gauges+per-1)/per;
                if((down&KEY_L)&&dialog_gauge_page>0)dialog_gauge_page--;
                if((down&KEY_R)&&dialog_gauge_page+1<pages)dialog_gauge_page++;
            }
            if((down&KEY_UP)&&dialog_pick>0){dialog_pick--;dialog_info=false;dialog_help_index=-1;}
            if((down&KEY_DOWN)&&dialog_pick+1<n){dialog_pick++;dialog_info=false;dialog_help_index=-1;}
            if(dialog_pick<dialog_scroll)dialog_scroll=dialog_pick;
            if(dialog_pick>=dialog_scroll+8)dialog_scroll=dialog_pick-7;
            if((down&KEY_TOUCH)&&tp->py>=36&&tp->py<180){
                dialog_pick=dialog_scroll+(tp->py-36)/18;
                if(dialog_pick>=n)dialog_pick=n-1;
                if(dialog_pick>=0){
                    int r=dialog_at(dialog_pick);
                    if(tp->px>=217 && r>=0 && def.refs[r].kind!=5 && def.refs[r].kind!=2)
                        toggle_favorite(def.refs[r].name,def.refs[r].label);
                    else dialog_action();
                }
            }
            if(down&KEY_X){
                int r=dialog_at(dialog_pick);
                if(r>=0&&def.refs[r].kind!=5&&def.refs[r].kind!=2)
                    toggle_favorite(def.refs[r].name,def.refs[r].label);
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
            if((down&KEY_TOUCH)&&tp->py>=36&&tp->py<180){
                list_pick=list_scroll+(tp->py-36)/18;
                if(list_pick>=tune_item_count())list_pick=tune_item_count()-1;
                if(list_pick>=0){
                    int idx=tune_item_at(list_pick);
                    if(tp->px>=217&&idx>=0 && def.menus[idx].target[0])
                        toggle_favorite(def.menus[idx].target,def.menus[idx].label);
                    else open_tune_item();
                }
            }
            if(down&KEY_X){
                int idx=tune_item_at(list_pick);
                if(idx>=0&&def.menus[idx].target[0])
                    toggle_favorite(def.menus[idx].target,def.menus[idx].label);
            }
            if(down&KEY_A)open_tune_item();
        }else{
            int total=category_count();
            if((down&KEY_UP)&&tune_cat>0){tune_cat--;list_pick=list_scroll=0;}
            if((down&KEY_DOWN)&&tune_cat+1<total){tune_cat++;list_pick=list_scroll=0;}
            if(down&KEY_A)tune_focus=1;
            if((down&KEY_TOUCH)&&tp->py>=36&&tp->py<180){
                tune_focus=1;list_pick=list_scroll+(tp->py-36)/18;
                if(list_pick>=tune_item_count())list_pick=tune_item_count()-1;
                if(list_pick>=0)open_tune_item();
            }
        }
    }else if(page==P_FAV){
        if(down&KEY_B){goto_page(P_HOME);return;}
        if(down&KEY_UP)navigate(-1);
        if(down&KEY_DOWN)navigate(1);
        if((down&KEY_TOUCH)&&tp->py>=36&&tp->py<180){
            list_pick=list_scroll+(tp->py-36)/18;
            if(list_pick>=favorite_count)list_pick=favorite_count-1;
            if(list_pick>=0){
                if(tp->px>=217)toggle_favorite(favorites[list_pick].target,favorites[list_pick].label);
                else open_target(favorites[list_pick].target,favorites[list_pick].label,true);
            }
        }
        if((down&KEY_X)&&list_pick<favorite_count)
            toggle_favorite(favorites[list_pick].target,favorites[list_pick].label);
        if((down&KEY_A)&&list_pick<favorite_count)
            open_target(favorites[list_pick].target,favorites[list_pick].label,true);
        if(list_pick>=favorite_count){list_pick=favorite_count?favorite_count-1:0;
            if(list_scroll>list_pick)list_scroll=list_pick;}
    }else if(page==P_LIVE){
        if(down&KEY_B){goto_page(view_from_tree?P_TUNE:P_HOME);return;}
        if(down&KEY_UP)navigate(-1);
        if(down&KEY_DOWN)navigate(1);
        if((down&KEY_TOUCH)&&tp->py>=28&&tp->py<156){
            list_pick=list_scroll+(tp->py-28)/16;
            if(list_pick>=def.channel_count)list_pick=def.channel_count-1;
        }
        if(down&KEY_Y){goto_page(P_GRAPH);return;}
        if(down&KEY_L)active_slot=(active_slot+SLOTS-1)%SLOTS;
        if(down&KEY_R)active_slot=(active_slot+1)%SLOTS;
        if((down&KEY_X)&&list_pick>=0&&list_pick<def.channel_count){
            const BtField*f=&def.channels[list_pick];
            if(f->scaled&&f->width*f->height==1){
                snprintf(channel_names[active_slot],sizeof(channel_names[active_slot]),"%s",f->name);
                clear_history(active_slot);refresh_idle_gauges();
                if(sd_ready)(void)bt_sd_gauges_save((const char (*)[64])channel_names);
            }
        }
    }else if(page==P_GRAPH){
        if(down&KEY_B){
            if(graph_from_ini && graph_from_dialog[0]){
                goto_page(P_TUNE);
                open_dialog(graph_from_dialog,graph_from_label);
                graph_from_ini=false;active_ini_graph=-1;
            }else goto_page(P_LIVE);
            return;
        }
        if(graph_from_ini && active_ini_graph>=0 && active_ini_graph<def.graph_count &&
           (down&KEY_X)){
            const BtIniGraph *g=&def.graphs[active_ini_graph];
            if(g->line_count>0){
                active_ini_graph_line=(active_ini_graph_line+1)%g->line_count;
                for(int slot=0;slot<SLOTS;slot++){
                    int idx=g->first_line+(active_ini_graph_line+slot)%g->line_count;
                    if(idx>=0&&idx<def.graph_line_count){
                        const char *ch=def.graph_lines[idx].channel;
                        snprintf(channel_names[slot],sizeof(channel_names[slot]),"%s",ch);
                        clear_history(slot);
                    }
                }
                refresh_graph_formulas();
            }
            return;
        }
        if(down&KEY_Y)graph_stats=!graph_stats;
        if(down&KEY_L)active_slot=(active_slot+SLOTS-1)%SLOTS;
        if(down&KEY_R)active_slot=(active_slot+1)%SLOTS;
        if((down&KEY_X)&&!graph_from_ini)goto_page(P_LIVE);
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
    bt_ini_reset(&def);reset_ref_index();
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
        /* START is handled in the active editor; it must never terminate the application. */
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
