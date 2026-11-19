/* Full-INI regression: a CurveEditor must always render as a flat X/Y line,
 * and a TableEditor must open as a flat 2D grid, never a 3D surface. */
#define main bts_dsi_entrypoint
#include "../source/main.c"
#undef main
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
void bt_async_cancel(void){}
bool bt_write_busy(void){return false;}
bool bt_write_cancel(void){return false;}
bool bts_sfx_play(int cue,bool ready){(void)cue;(void)ready;return false;}
const char *bts_sfx_status(bool ready){(void)ready;return "SFX";}
bool bts_sfx_files_ready(bool ready){(void)ready;return false;}
static uint16_t pixels2d[W*H];
static void load_ini(const char *path){
    FILE *f=fopen(path,"rb");assert(f);
    uint8_t block[3072];size_t n;
    bt_ini_reset(&def);
    while((n=fread(block,1,sizeof block,f)))bt_ini_feed(&def,block,n);
    fclose(f);assert(bt_ini_finish(&def));
    assert(!def.truncated && !def.out_of_memory);
    ini_ok=true;build_map_index();
}
static void prepare_values(void){
    cal_load.active=false;table.valid=true;
    table.has_x=true;table.x_count=table.width;
    table.has_y=table.height>1;table.y_count=table.height;
    for(int x=0;x<table.width;x++)table.xbins[x]=x*x*10+50;
    for(int y=0;y<table.height;y++)table.ybins[y]=y*20;
    for(int y=0;y<table.height;y++)for(int x=0;x<table.width;x++)
        table.cells[y*table.width+x]=30+20*sin(x*0.43)+y;
}
int main(int argc,char **argv){
    assert(argc==2);load_ini(argv[1]);
    assert(curve_plot_x(10,0,100,33,243)==54);
    assert(curve_plot_x(90,0,100,33,243)==222);
    assert(curve_plot_x(1000,0,100,33,243)==243);
    assert(curve_plot_x(-50,0,100,33,243)==33);
    /* A CurveEditor is intrinsically two-dimensional; no 3D mode, ever. */
    int curve=map_for_target("fan1PwmCurve");assert(curve>=0);
    assert(def.maps[curve].curve);
    map_surface_3d=true;open_map(curve);
    assert(!map_surface_3d);prepare_values();assert(table.height==1);
    memset(top,0,W*H*sizeof(uint16_t));map_top();memcpy(pixels2d,top,sizeof pixels2d);
    map_surface_3d=true;memset(top,0,W*H*sizeof(uint16_t));map_top();
    assert(memcmp(pixels2d,top,sizeof pixels2d)==0);
    /* The TableEditor has two distinct views. The initial view must be flat.
     * A touch of the view switch changes perspective without losing cells. */
    int table_id=map_for_target("veTableTbl");assert(table_id>=0);
    assert(!def.maps[table_id].curve);
    open_map(table_id);assert(!map_surface_3d);
    prepare_values();assert(table.width>1 && table.height>1);
    memset(top,0,W*H*sizeof(uint16_t));map_top();memcpy(pixels2d,top,sizeof pixels2d);
    const double original=table.cells[0];
    touchPosition touch={.px=185,.py=29};
    handle_buttons(KEY_TOUCH,0,&touch);
    assert(map_surface_3d && table.cells[0]==original);
    memset(top,0,W*H*sizeof(uint16_t));map_top();
    assert(memcmp(pixels2d,top,sizeof pixels2d)!=0);
    handle_buttons(KEY_TOUCH,0,&touch);
    assert(!map_surface_3d && table.cells[0]==original);
    memset(top,0,W*H*sizeof(uint16_t));map_top();
    assert(memcmp(pixels2d,top,sizeof pixels2d)==0);
    printf("PASS v30: CurveEditor flat 2D X/Y; TableEditor opens flat 2D; optional 3D toggle; real X spacing\n");
    bt_ini_reset(&def);return 0;
}
