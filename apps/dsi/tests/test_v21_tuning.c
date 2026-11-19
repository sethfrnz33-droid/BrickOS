/* Test actual DSi input paths, not a separate reimplementation. */
#define main brick_dsi_boot_stub
#include "../source/main.c"
#undef main
#include <assert.h>
#include <stdio.h>
#include <math.h>
bool bts_sfx_play(int cue,bool sd){(void)cue;(void)sd;return true;}
void bts_sfx_shutdown(void){}
bool bts_sfx_files_ready(bool sd){(void)sd;return true;}
const char *bts_sfx_status(bool sd){(void)sd;return "AUDIO OK";}

int main(void){
    bt_ini_reset(&def);
    BtField field={0};
    strcpy(field.name,"testField");
    field.width=1;field.height=1;field.offset=0;field.page=1;
    field.type=BT_U16;field.kind=0;field.scale=0.1;field.scaled=true;
    field.low=0;field.high=100;field.limits_known=true;
    def.valid=true;def.constants=&field;def.constant_count=1;
    ini_ok=true;
    table.valid=true;table.width=1;table.height=1;table.raw_length=2;
    table.baseline[0]=100;table.baseline[1]=0; /* 10.0 */
    memcpy(table.edited,table.baseline,2);
    table.cells[0]=10.0;chosen_map=BTS_MAX_MAPS;
    strcpy(scalar_map.z,"testField");
    active_fields[0]=field;active_field_present[0]=true;
    keypad_begin();assert(keypad_open);
    keypad_key(7); /* CLEAR */
    keypad_key(8);keypad_key(9);keypad_key(12);keypad_key(5); /* 12.5 */
    keypad_key(14); /* APPLY (local only) */
    assert(!keypad_open && table.dirty && fabs(table.cells[0]-12.5)<0.01);
    assert(table.baseline[0]==100&&table.edited[0]==125);
    keypad_begin();keypad_key(7);keypad_key(1);keypad_key(1);keypad_key(1); /* 888 */
    keypad_key(14);
    assert(keypad_open && table.edited[0]==125); /* INI range protected */
    keypad_key(15);assert(!keypad_open);
    assert(table.dirty); /* staging is not ECU commit */
    chosen_map=-1;table.valid=false;page=P_HOME;ini_ok=true;
    frame=700;activity_frame=0;
    update_idle();assert(idle_dashboard);
    touchPosition tp={0};
    handle_buttons(KEY_A,0,&tp); /* first key only wakes UI */
    assert(!idle_dashboard&&page==P_HOME);
    handle_buttons(KEY_A,0,&tp); /* next key actually opens menu */
    assert(page==P_TUNE);
    open_dialog("parent","Main panel");
    open_dialog("child","Nested panel");
    assert(dialog_depth==1&&!strcmp(dialog_name,"child"));
    handle_buttons(KEY_B,0,&tp);
    assert(dialog_depth==0&&!strcmp(dialog_name,"parent"));
    handle_buttons(KEY_B,0,&tp);
    assert(!dialog_name[0]);
    assert(bt_sd_init());
    char requested[2][64]={{"MAPValue"},{"AFRValue"}};
    assert(bt_sd_gauges_save((const char (*)[64])requested));
    char loaded[2][64]={{0},{0}};
    assert(bt_sd_gauges_load(loaded));
    assert(!strcmp(loaded[0],requested[0])&&!strcmp(loaded[1],requested[1]));
    puts("PASS: INI-bounded keypad stages without writing, safe wake, persisted gauge settings");
    /* Reset before freeing since field belongs to this stack, not malloc. */
    def.constants=NULL;def.constant_count=0;
    return 0;
}
