#define main brick_dsi_startup
#include "../source/main.c"
#undef main
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
bool bts_sfx_play(int cue,bool sd){(void)cue;(void)sd;return true;}
void bts_sfx_shutdown(void){}
bool bts_sfx_files_ready(bool sd){(void)sd;return true;}
const char *bts_sfx_status(bool sd){(void)sd;return "SFX OK";}

static const char *ini=
"[TunerStudio]\n"
"signature = \"TEST-ECU-3300\"\n"
"[Constants]\n"
"pageIdentifier = \"\\x00\\x01\"\n"
"pageSize = 2048\n"
"page = 1\n"
"rpmAxis = array, U16, 4, [3], RPM, 1, 0\n"
"loadAxis = array, U16, 12, [2], kPa, 1, 0\n"
"boostTarget = scalar, U16, 16, kPa, 0.1, 0, 0, 300\n"
"ecuMode = bits, U08, 20, [0:1], \"Stop\", \"Run\", \"Test\"\n"
"veTable = array, U16, 32, [3x2], %, 0.1, 0, 0, 200\n"
"largeTable = array, U16, 100, [20x16], %, 0.1, 0, 0, 200\n"
"rejectField = scalar, U16, 60, kPa, 1, 0\n"
"[OutputChannels]\n"
"RPMValue = scalar, U16, 0, RPM, 1, 0\n"
"[TableEditor]\n"
"table = veTable, fuel3d, \"Fuel VE\"\n"
"xBins = rpmAxis, RPMValue\n"
"yBins = loadAxis, RPMValue\n"
"zBins = veTable\n"
"table = largeTable, large3d, \"Large Table\"\n"
"zBins = largeTable\n"
"[UserDefined]\n"
"dialog = settings\n"
"field = \"Boost Target\", boostTarget\n"
"field = \"ECU Mode\", ecuMode\n"
"[Menu]\n"
"menuDialog = main\n"
"menu = Fuel\n"
"subMenu = veTable, \"VE Table\"\n"
"subMenu = boostTarget, \"Boost\"\n"
"subMenu = settings, \"Engine Settings\"\n"
"subMenu = largeTable, \"Large Table\"\n"
"subMenu = rejectField, \"Rejected Field\"\n";

static void pump_cal(void){
  for(int i=0;i<3000 && cal_load.active;i++){
    ++frame;
    service_network();
    usleep(2000);
  }
  if(cal_load.active)fprintf(stderr,"timeout stage=%d offset=%d total=%d err=%s\n",cal_load.stage,cal_load.offset,cal_load.total,note);
  assert(!cal_load.active);
  assert(table.valid);
  assert(chosen_map>=0);
}
int main(void){
  bt_ini_reset(&def);
  bt_ini_feed(&def,(const uint8_t*)ini,strlen(ini));
  assert(bt_ini_finish(&def));
  sd_ready=bt_sd_init();
  assert(sd_ready);
  FILE *cached_ini=fopen(bt_sd_cache_path(),"wb");
  assert(cached_ini);
  assert(fwrite(ini,1,strlen(ini),cached_ini)==strlen(ini));
  assert(fclose(cached_ini)==0);
  build_map_index();set_compat();
  assert(def.map_count==2&&map_count==2);
  assert(def.menu_count==6 && def.ref_count==2);
  wifi_up=ecu_up=bridge_up=true;
  page=P_TUNE;tune_cat=0;tune_focus=1;list_pick=0;
  open_tune_item();
  assert(cal_load.active&&chosen_map==0&&!table.valid);
  pump_cal();
  assert(table.width==3&&table.height==2);
  assert(table.has_x&&table.x_count==3&&table.has_y&&table.y_count==2);
  assert(table.xbins[2]==3000&&table.ybins[1]==100);
  assert(fabs(table.cells[5]-15.0)<0.001);
  assert(set_map_cell(16.0));assert(table.dirty&&table.baseline[0]==100);
  /* Staged axis tuning must preserve the original ECU bytes and never mix
   * X/Y edits into the Z array. SELECT switches the focused INI field. */
  touchPosition tp={0};
  handle_buttons(KEY_SELECT,0,&tp);assert(map_focus==1);
  assert(set_map_cell(1200));assert(table.axis_dirty[0]);
  assert(table.xbins[0]==1200 && table.axis_baseline[0][0]==0xe8);
  handle_buttons(KEY_SELECT,0,&tp);assert(map_focus==2);
  assert(set_map_cell(75));assert(table.axis_dirty[1]);
  assert(table.ybins[0]==75&&table.axis_baseline[1][0]==50);
  assert(any_map_dirty());
  handle_buttons(KEY_B,0,&tp);assert(abandon_confirm&&chosen_map==0);
  handle_buttons(KEY_B,0,&tp);assert(chosen_map==-1&&!table.valid);
  map_focus=0;
  chosen_map=-1;table.valid=false;
  list_pick=1;open_tune_item();
  assert(cal_load.active&&chosen_map==BTS_MAX_MAPS);
  pump_cal();
  assert(table.width==1&&table.height==1);
  assert(fabs(table.cells[0]-12.3)<0.001);
  assert(set_map_cell(15.0));assert(table.dirty);
  chosen_map=-1;table.valid=false;
  list_pick=2;open_tune_item();
  assert(!strcmp(dialog_name,"settings"));
  dialog_pick=1;dialog_action();
  assert(cal_load.active&&chosen_map==BTS_MAX_MAPS);
  pump_cal();
  assert(table.cells[0]==2);
  assert(set_map_cell(1));assert(table.edited[0]==0xa9); /* preserve other bits of 0xaa */
  assert(table.baseline[0]==0xaa);
  chosen_map=-1;table.valid=false;
  list_pick=3;open_tune_item();
  assert(cal_load.active);
  pump_cal();
  assert(table.width==20&&table.height==16);
  assert(fabs(table.cells[319] - 41.9)<0.01);
  chosen_map=-1;table.valid=false;
  list_pick=4;open_tune_item();
  assert(cal_load.active);
  for(int i=0;i<3000 && cal_load.active;i++){frame++;service_network();usleep(2000);}
  assert(!cal_load.active&&!table.valid&&chosen_map==-1);
  assert(strstr(note,"ECU busy")!=NULL);
  puts("PASS v2.3: map/scalar/enum open, X/Y editing, 640-byte multi-chunk reads, visible 503 rejection");
  return 0;
}
