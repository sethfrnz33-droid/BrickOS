/* Real Proteus INI regression: View dialogs are full grouped live dashboards,
 * not lists of fake "Graph" navigation targets. */
#define main bts_dsi_entrypoint
#include "../source/main.c"
#undef main
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void bt_async_cancel(void){}
void bt_async_forget_connection(void){}
bool bt_async_busy(void){return false;}
BtNetJob bt_async_job(void){return BT_NET_NONE;}
const char *bt_async_phase(void){return "IDLE";}
size_t bt_async_bytes(void){return 0;}
size_t bt_async_saved_bytes(void){return 0;}
size_t bt_async_parsed_bytes(void){return 0;}
size_t bt_cache_bytes(void){return 0;}
bool bt_write_busy(void){return false;}
bool bt_write_cancel(void){return false;}
bool bts_sfx_play(int cue,bool ready){(void)cue;(void)ready;return false;}
const char *bts_sfx_status(bool ready){(void)ready;return "SFX";}
bool bts_sfx_files_ready(bool ready){(void)ready;return false;}

static void parse_ini(const char *path){
    FILE *f=fopen(path,"rb");assert(f);
    uint8_t chunk[2537];size_t size;
    bt_ini_reset(&def);
    while((size=fread(chunk,1,sizeof chunk,f)))bt_ini_feed(&def,chunk,size);
    fclose(f);
    assert(bt_ini_finish(&def));
    assert(def.valid&&!def.truncated&&!def.out_of_memory);
    ini_ok=true;build_map_index();
    assert(def.graph_count==103&&def.graph_line_count==341);
    int view=0,unsupported=0;
    for(int i=0;i<def.menu_count;i++){
        if(def.menus[i].group==0){
            view=!strcasecmp(def.menus[i].label,"View");continue;
        }
        if(view&&def.menus[i].target[0]){
            if(!monitor_has_target(def.menus[i].target)){
                fprintf(stderr,"VIEW WITHOUT MONITOR: %s -> %s\n",def.menus[i].label,def.menus[i].target);
                unsupported++;
            }
        }
    }
    assert(unsupported==0);
}
static int expected_groups(const char*owner){
    int n=0;
    for(int i=0;i<def.graph_count;i++)if(!strcasecmp(def.graphs[i].owner,owner))n++;
    return n;
}
static int count_lines_current(void){
    int total=0;
    for(int i=monitor_page*BTS_MONITOR_PER_PAGE;
        i<monitor_graph_total&&i<monitor_page*BTS_MONITOR_PER_PAGE+BTS_MONITOR_PER_PAGE;i++)
        total+=def.graphs[monitor_graph_ids[i]].line_count;
    return total;
}
static void assert_rendered(void){
    idle_dashboard=false;
    render();
    int a=0,b=0;
    for(int i=0;i<W*H;i++){
        if(top[i]==HIGHLIGHT || top[i]==WHITE)a++;
        if(bottom[i]==HIGHLIGHT || bottom[i]==WHITE)b++;
    }
    assert(a>70&&b>70);
}
static void check_open(const char *owner,const char *label){
    goto_page(P_TUNE);tune_cat=3;tune_focus=1;list_pick=2;list_scroll=1;
    open_target(owner,label,true);
    assert(page==P_MONITOR);
    assert(!dialog_name[0]); /* no nested fake 'Graph' settings */
    assert(!strcmp(monitor_owner,owner));
    assert(monitor_graph_total==expected_groups(owner));
    assert(monitor_page==0);
    assert(monitor_line_total==count_lines_current());
    assert_rendered();
    int page_count=monitor_page_total();
    for(int p=1;p<page_count;p++){
        handle_buttons(KEY_R,0,NULL);
        assert(monitor_page==p);
        if(p<monitor_graph_pages())assert(monitor_line_total==count_lines_current());
        else assert(monitor_line_total==0);
        assert_rendered();
    }
    handle_buttons(KEY_B,0,NULL);
    assert(page==P_TUNE);
    assert(tune_cat==3&&tune_focus==1&&list_pick==2&&list_scroll==1);
}
int main(int argc,char **argv){
    assert(argc==2);
    parse_ini(argv[1]);
    check_open("fuel_computerDialog","fuel_computer");
    open_target("fuel_computerDialog","Fuel computer",true);
    assert(monitor_graph_total==4&&monitor_line_total==15);
    assert(monitor_page_total()>=1);
    for(int k=0;k<4;k++){
        const BtIniGraph *g=&def.graphs[monitor_graph_ids[k]];
        assert(g->line_count>=3&&g->line_count<=5);
        /* This generic INI graph label is metadata, not a separate setting. */
        assert(!strcmp(g->title,"Graph"));
    }
    /* live length must include ALL visible direct channels, not only two. */
    live.valid=true;live.length=BTS_LIVE_CAP;frame=100;live_frame=100;
    wifi_up=ecu_up=true;
    memset(live.bytes,0,sizeof live.bytes);
    int request=live_length(),required=0,direct=0;
    for(int i=0;i<monitor_line_total;i++){
        const BtField *f=bt_field(&def,monitor_line_names[i],true);
        if(!f||!f->scaled)continue;
        int end=f->offset+bt_type_size(f->type);
        if(end>required&&end<=BTS_LIVE_CAP)required=end;
        direct++;
    }
    assert(direct>=10&&request>=required);
    assert_rendered();
    /* Recording a real sample must populate all visible lines, then X
     * switches from grouped readout values to grouped simultaneous trends. */
    monitor_collect_sample();
    assert(monitor_sample_count==1);
    monitor_collect_sample();
    assert(monitor_sample_count==2);
    bool has_sample=false;
    for(int i=0;i<monitor_line_total;i++)
        for(int j=0;j<2;j++)has_sample|=monitor_sample_ok[i][j]!=0;
    assert(has_sample);
    handle_buttons(KEY_X,0,NULL);
    assert(monitor_trends);
    assert_rendered();
    handle_buttons(KEY_X,0,NULL);
    assert(!monitor_trends);
    assert_rendered();
    handle_buttons(KEY_B,0,NULL);
    check_open("knock_controllerDialog","knock_controller");
    /* Three graph pages contain all 10 graphLine groups; fourth page
     * contains the panel's INI boolean indicators without extra submenus. */
    open_target("knock_controllerDialog","Knock controller",true);
    assert(monitor_graph_total==10);
    assert(monitor_graph_pages()==3);
    assert(monitor_indicator_total>=2);
    for(int i=0;i<3;i++)handle_buttons(KEY_R,0,NULL);
    assert(monitor_status_mode());assert(monitor_page==3);
    assert_rendered();
    handle_buttons(KEY_L,0,NULL);
    assert(monitor_page==2&&!monitor_status_mode());
    handle_buttons(KEY_B,0,NULL);
    /* Favorites may point directly to a View target and must return there. */
    goto_page(P_FAV);
    favorite_return=true;
    open_target("fuel_computerDialog","fuel_computer",true);
    assert(page==P_MONITOR&&monitor_from_favorite);
    handle_buttons(KEY_B,0,NULL);
    assert(page==P_FAV);
    /* Full-INI aggregate: every graph and its membership survives parsing. */
    int references=0;
    for(int i=0;i<def.graph_count;i++){
        assert(def.graphs[i].owner[0]);
        assert(def.graphs[i].line_count>0);
        references+=def.graphs[i].line_count;
    }
    assert(references==341);
    printf("PASS v31: 103 groups / 341 graph lines; joined View dashboards, 2-screen layouts, page navigation, indicators, Favorites, 15-channel fuel view, subscribed live bytes\n");
    bt_ini_reset(&def);
    return 0;
}
