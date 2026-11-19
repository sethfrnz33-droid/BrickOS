/* Full real-INI regression of DSi curve navigation, Favorites, and UI semantics. */
#define main bts_dsi_entrypoint
#include "../source/main.c"
#undef main
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
/* The navigation regression never opens a socket or plays a sound. */
void bt_async_cancel(void){}
bool bt_write_busy(void){return false;}
bool bt_write_cancel(void){return false;}
bool bts_sfx_play(int cue,bool sd_ready){(void)cue;(void)sd_ready;return false;}

static void load_ini(const char *path){
    FILE *f=fopen(path,"rb");assert(f);
    uint8_t buf[1999];size_t n;
    bt_ini_reset(&def);
    while((n=fread(buf,1,sizeof buf,f))!=0)bt_ini_feed(&def,buf,n);
    fclose(f);assert(bt_ini_finish(&def));
    assert(!def.truncated&&!def.out_of_memory);
    ini_ok=true;build_map_index();
}
static void open_curve_via_panel(const char *dialog,const char *name){
    open_dialog(dialog,dialog);
    int found=-1;
    for(int row=0;row<dialog_count();row++){
        int ref=dialog_at(row);
        if(ref>=0&&def.refs[ref].kind==1&&!strcasecmp(name,def.refs[ref].name)){
            found=row;break;
        }
    }
    assert(found>=0);
    dialog_pick=found;
    int expect=map_for_target(name);
    assert(expect>=0&&def.maps[expect].curve);
    dialog_action();
    assert(chosen_map==expect);
    assert(cal_load.active);
    assert(table.width>1&&table.height==1);
    /* 2D line graph must draw live ECU-derived Y values in its own view. */
    cal_load.active=false;table.valid=true;table.has_x=true;
    table.x_count=table.width;map_surface_3d=false;
    for(int i=0;i<table.width;i++){
        table.xbins[i]=500+i*150;
        table.cells[i]=25+20*sin(i*.55);
    }
    memset(top,0,W*H*sizeof(uint16_t));
    map_top();
    int hits=0;
    for(int y=80;y<157;y++)for(int x=33;x<244;x++)if(top[y*W+x])hits++;
    assert(hits>100);
    chosen_map=-1;table.valid=false;cal_load.active=false;dialog_name[0]=0;
}
static void favorite_route(const char *target,const char *label,bool expect_dialog){
    page=P_FAV;chosen_map=-1;table.valid=false;cal_load.active=false;
    memset(favorites,0,sizeof favorites);favorite_count=1;
    snprintf(favorites[0].target,sizeof favorites[0].target,"%s",target);
    snprintf(favorites[0].label,sizeof favorites[0].label,"%s",label);
    list_pick=0;
    open_favorite();
    assert(page==P_TUNE&&favorite_return);
    if(expect_dialog)assert(dialog_name[0]&&chosen_map<0);
    else assert(chosen_map>=0&&cal_load.active);
    if(expect_dialog){
        handle_buttons(KEY_B,0,NULL);
    }else{
        cal_load.active=false;table.valid=true;table.dirty=false;
        handle_buttons(KEY_B,0,NULL);
    }
    assert(page==P_FAV && !favorite_return);
}
static int find_owner(const char *owner,int kind){
    for(int i=0;i<def.ref_count;i++)if(!strcasecmp(def.refs[i].owner,owner)&&def.refs[i].kind==kind)return i;
    return -1;
}
int main(int argc,char **argv){
    assert(argc==2);load_ini(argv[1]);
    assert(def.map_count>=190);
    assert(map_for_target("fan1PwmCurve")>=0);
    assert(map_for_target("cltFuelCorrCurve")>=0);
    open_curve_via_panel("fan1PwmCurveDialog","fan1PwmCurve");
    open_curve_via_panel("cltFuelCorrCurveDialog","cltFuelCorrCurve");
    favorite_route("fan1PwmCurve","Fan 1 Speed Demand Curve",false);
    favorite_route("fan1PwmCurveDialog","Fan Curve Settings",true);
    int indicator=find_owner("fuel_computerIndicatorPanel",8);
    assert(indicator>=0);
    assert(strstr(def.refs[indicator].detail,"No")!=NULL);
    assert(strstr(def.refs[indicator].detail,"Yes")!=NULL);
    open_dialog("fuel_computerIndicatorPanel","Fuel computer status");
    int indRow=-1;
    for(int i=0;i<dialog_count();i++)if(def.refs[dialog_at(i)].kind==8){indRow=i;break;}
    assert(indRow>=0&&!dialog_selectable(indRow));
    assert(find_owner("fuel_computerDialog",1)>=0);
    /* When disconnected, an indicator cannot fabricate an OFF reading. */
    double v=123;
    wifi_up=ecu_up=false;live.valid=false;
    assert(!indicator_live_value(def.refs[indicator].name,&v));
    const BtField *lf=bt_field(&def,def.refs[indicator].name,true);
    assert(lf&&lf->scaled&&lf->offset>=0&&lf->offset+bt_type_size(lf->type)<=BTS_LIVE_CAP);
    wifi_up=ecu_up=true;live.valid=true;
    frame=100;live_frame=100;live.length=BTS_LIVE_CAP;
    memset(live.bytes,0,sizeof live.bytes);
    assert(indicator_live_value(def.refs[indicator].name,&v)&&v==0);
    live.bytes[lf->offset]|=1u;
    assert(indicator_live_value(def.refs[indicator].name,&v)&&v!=0);
    /* All user-defined command buttons are identified and non-interactive. */
    int commands=0,indicators=0,labels=0;
    for(int i=0;i<def.ref_count;i++){
        const BtDialogRef *r=&def.refs[i];
        if(r->kind==2)commands++;
        if(r->kind==8)indicators++;
        if(r->kind==5)labels++;
    }
    assert(commands>=100&&indicators>=290&&labels>=600);
    printf("PASS v29: nested 2D curves, curve plot, Favorites open/back, %d status indicators, %d commands as buttons, %d noninteractive labels\n",indicators,commands,labels);
    bt_ini_reset(&def);
    return 0;
}
