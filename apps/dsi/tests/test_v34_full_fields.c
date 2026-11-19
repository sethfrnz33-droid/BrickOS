/* Source-grounded DSi input/output & string editor regression against the
 * uploaded generated Proteus F7 INI. Host only: no ECU commands are sent. */
#define main bts_dsi_entrypoint
#include "../source/main.c"
#undef main
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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

static void load_ini(const char*path){
    FILE*f=fopen(path,"rb");assert(f);
    bt_ini_reset(&def);uint8_t b[3073];size_t n;
    while((n=fread(b,1,sizeof b,f)))bt_ini_feed(&def,b,n);
    fclose(f);
    assert(bt_ini_finish(&def));assert(!def.truncated&&!def.out_of_memory);
    ini_ok=true;
    assert(bt_sd_init());sd_ready=true;
    FILE*src=fopen(path,"rb"),*dest=fopen(bt_sd_cache_path(),"wb");
    assert(src&&dest);
    uint8_t bytes[4096];size_t read;
    while((read=fread(bytes,1,sizeof bytes,src)))assert(fwrite(bytes,1,read,dest)==read);
    fclose(src);assert(fclose(dest)==0);
    build_map_index();
}
static int owner_count(const char*owner,int kind){
    int n=0;for(int i=0;i<def.ref_count;i++)if(def.refs[i].kind==kind &&
        !strcasecmp(def.refs[i].owner,owner))n++;
    return n;
}
static int owner_ref(const char*owner,const char*name){
    for(int i=0;i<def.ref_count;i++)if(!strcasecmp(def.refs[i].owner,owner)&&
        !strcasecmp(def.refs[i].name,name))return i;
    return -1;
}
int main(int argc,char**argv){
    assert(argc==2);load_ini(argv[1]);
    assert(def.constant_count>=1960); /* 19+ ASCII strings and 5 I2C fields */
    assert(owner_count("allPinsOutputs_1",0)==24);
    assert(owner_count("allPinsOutputs_2",0)==24);
    assert(owner_count("allPinsOutputs_3",0)==8);
    assert(owner_count("allPinsInputs_Analog",0)==14);
    assert(owner_count("allPinsInputs_Digital",0)==13);
    assert(owner_ref("tpsSent1","EtbSentInput")>=0);
    assert(owner_ref("sdLoggingStatePanel","sdLoggingState")>=0);
    assert(owner_ref("ignBlend1Readouts","RPMValue")>=0);
    assert(owner_count("clt_thermistor",9)==1);
    assert(owner_count("iat_thermistor",9)==1);
    assert(owner_count("egoSettings_sensor",9)==1);
    assert(!dialog_selectable(-1));
    const BtField*ascii=bt_field(&def,"engineMake",false);
    assert(ascii&&ascii->kind==3&&ascii->type==BT_ASCII&&ascii->width==32);
    const BtField*pin=bt_field(&def,"ignitionPins1",false);
    const BtField*input=bt_field(&def,"clutchDownPin",false);
    const BtField*i2c=bt_field(&def,"i2c1_speed",false);
    assert(pin&&pin->kind==2&&input&&input->kind==2);
    assert(i2c&&i2c->kind==2&&i2c->offset==3864&&i2c->bit_hi==2);
    assert(bt_field(&def,"i2c2_speed",false)->offset==3876);
    assert(bt_field(&def,"i2c3_speed",false)->offset==3888);
    assert(bt_field(&def,"i2c4_speed",false)->offset==3900);
    assert(bt_field(&def,"pca96851_i2c_bus",false)->offset==3912);
    char labels[256][36];
    int count=bt_bit_choices(argv[1],"i2c1_speed",labels,256);
    assert(count==8&&strcmp(labels[0],"Standart mode 100K")==0);
    count=bt_bit_choices(argv[1],"ignitionPins1",labels,256);
    assert(count==128&&labels[22][0]&&labels[119][0]==0);
    count=bt_bit_choices(argv[1],"clutchDownPin",labels,256);
    assert(count==128&&labels[40][0]);
    static char large_enum[512][36];
    count=bt_bit_choices(argv[1],"sentInputPins1",large_enum,512);
    assert(count==512 && !strcmp(large_enum[0],"NONE") &&
           !strcmp(large_enum[2],"PA0") && large_enum[511][0]==0);
    /* Exhaustively verify that EVERY distinct, INI-referenced bitfield has
     * a resolvable, nonempty enum; missing choices must not become a fake
     * numeric editor or a silently truncated pin list. */
    int all_bitfields=0,missing_enums=0;
    for(int i=0;i<def.ref_count;i++)if(def.refs[i].kind==0){
        const BtField*f=bt_field(&def,def.refs[i].name,false);
        if(!f||f->kind!=2)continue;
        bool visited=false;
        for(int j=0;j<i;j++)if(def.refs[j].kind==0&&
           !strcasecmp(def.refs[j].name,f->name)){visited=true;break;}
        if(visited)continue;
        all_bitfields++;
        int choices=bt_bit_choices(argv[1],f->name,large_enum,512);
        int valid=0;for(int j=0;j<choices;j++)valid+=large_enum[j][0]!=0;
        if(!choices||!valid){fprintf(stderr,"UNRESOLVED %s\n",f->name);missing_enums++;}
    }
    assert(all_bitfields>=640 && missing_enums==0);
    /* Bitfield edits preserve all unrelated bits, for shared registers. */
    BtField relative=*pin;relative.offset=0;
    uint8_t data[2]={0x80,0xFE};
    double before=0,after=0;
    assert(bt_field_value(&def,&relative,data,2,0,&before));
    assert(bt_encode_value(&def,&relative,data,2,0,22));
    assert(bt_field_value(&def,&relative,data,2,0,&after));
    assert(after==22&&data[1]==0xFE&&(data[0]&0x80)==0);
    /* A text field remains a byte array, not a fake floating-point scalar. */
    relative=*ascii;relative.offset=0;
    uint8_t phrase[32]={0};memcpy(phrase,"BMW",3);
    assert(bt_field_value(&def,&relative,phrase,32,0,&before)&&before=='B');
    assert(bt_encode_value(&def,&relative,phrase,32,1,'R'));
    assert(phrase[0]=='B'&&phrase[1]=='R'&&phrase[2]=='W');
    assert(!bt_encode_value(&def,&relative,phrase,32,0,200));
    /* Primary input/output controls open the REAL staged calibration editor. */
    goto_page(P_TUNE);
    open_dialog("outputsDialog","Outputs");
    assert(dialog_count()>=2);
    assert(open_ini_field("ignitionPins1","Ignition Output 1",false));
    assert(chosen_map==BTS_MAX_MAPS&&cal_load.active&&bit_visible_count>=30);
    cal_load.active=false;
    assert(open_ini_field("engineMake","Engine Make",false));
    assert(chosen_map==BTS_MAX_MAPS&&active_fields[0].kind==3);
    cal_load.active=false;table.valid=true;
    memset(table.baseline,0,table.raw_length);
    memcpy(table.baseline,"BMW",3);
    memcpy(table.edited,table.baseline,table.raw_length);
    for(int i=0;i<table.width;i++)table.cells[i]=table.baseline[i];
    cx=1;assert(set_map_cell('R'));
    assert(table.dirty&&table.edited[1]=='R');
    render();
    int hits=0;for(int i=0;i<W*H;i++)if(top[i]==HIGHLIGHT)hits++;
    assert(hits>10);
    printf("PASS v34: %d constants, %d refs, %d/%d bitfield options, output/input groups, I2C fixes, text editor, readoutPanel, 3 presets; ECU writes not exercised\n",
        def.constant_count,def.ref_count,all_bitfields-missing_enums,all_bitfields);
    bt_ini_reset(&def);return 0;
}
