#!/usr/bin/env python3
"""Native tests of exact v3.9 project functions: INI alias, nonzero Z, idle dedup.
Host tests do not emulate the 3DS renderer or a real ECU.
"""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()

def function(name):
    m=re.search(r'static\s+(?:void|bool|int|double|Field\s*\*)\s+'+re.escape(name)+r'\([^;]*\)\s*\{',s)
    assert m,name
    depth=0
    for i in range(s.index('{',m.start()),len(s)):
        if s[i]=='{':depth+=1
        elif s[i]=='}':
            depth-=1
            if depth==0:return s[m.start():i+1]
    raise ValueError(name)

assert 'map_3d_view=from_3d_ini;' in function('open_target')
assert 'map_3d_view=from_3d_ini;' in function('tune_enter_preview')
assert 'map_3d_view=from_3d_ini;' in function('enter_selected')
assert 'map_3d_view=!map_3d_view;' not in s
assert 'map_3d_view=false;' in function('open_cal')
assert 'if(map_3d_view){' in function('draw_map_deck')
assert 'bool idle_tune_page=(ui_page==PAGE_HUB' in s
assert 'if(tune_screen_asleep&&ui_page!=PAGE_TUNE_TREE&&ui_page!=PAGE_SOUND_TEST)' in function('draw_bottom')
assert 'map3d_heat(heat,light)' in function('draw_map_3d')
assert 'ECU data length differs from INI definition' in function('draw_map_3d')

preamble=r'''
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#define CHAN_CAP 64
#define RES_CAP 128
#define GAUGE_SLOTS 9
#define INI_GRAPH_LINES 6
enum {T_U08,T_S08,T_U16,T_S16,T_U32,T_S32,T_F32,T_F64,T_BAD};
enum {K_SCALAR,K_ARRAY,K_BITS};
typedef struct {char name[60],unit[26];int page,offset,width,height,kind,type,bytes,bitlo,bithi;double scale,translate,low,high;bool validScale,limitsKnown,powerCycle;} Field;
typedef struct {char id[64],alt[64],label[76],x[60],y[60],z[60];bool curve;} TableDef;
typedef struct {char owner[64],ref[64],label[60];int panel;} DialogRef;
typedef struct {char owner[64],id[64],title[64],channels[INI_GRAPH_LINES][64];int count;} IniGraphDef;
typedef struct {char label[72],command[64];int field,type;} Resolved;
static TableDef tables[4];static int table_count=0;
static Field cal[8],channels[CHAN_CAP];static int cal_count=0,chan_count=0;
static DialogRef refs[4];static int ref_count=0;
static IniGraphDef ini_graphs[4];static int ini_graph_count=0;
static Resolved results[RES_CAP];static int result_count=0;
static int gauge_fields[GAUGE_SLOTS],selectable_channels[CHAN_CAP];
static int selectable_count=0,live_length=512;
static bool ini_big_endian=false;
static const char *metric_fields[]={"RPMValue","MAPValue","AFRValue","TPSValue","coolant","intake","VBatt","ignitionAdvanceCyl1","oilPressure"};
static int dsize(int t){return t<=T_S08?1:t<=T_S16?2:t<=T_F32?4:t==T_F64?8:0;}
static Field *lookup(char *name,bool runtime){Field *array=runtime?channels:cal;int n=runtime?chan_count:cal_count;for(int i=0;i<n;i++)if(!strcasecmp(name,array[i].name))return &array[i];return NULL;}
static int field_index(const char *name){for(int i=0;i<cal_count;i++)if(!strcasecmp(cal[i].name,name))return i;return -1;}
static bool has_field_result(int id){for(int i=0;i<result_count;i++)if(results[i].field==id)return true;return false;}
static void add_result(const char*label,int fid,int typ){if(fid<0||fid>=cal_count||result_count>=RES_CAP||has_field_result(fid))return;Resolved*r=&results[result_count++];snprintf(r->label,sizeof(r->label),"%s",(label&&*label)?label:cal[fid].name);r->field=fid;r->type=typ;}
static void add_action_result(const char*label,const char*command){(void)label;(void)command;}
static bool icon_has(const char*name,const char*needle){
 if(!name)return false;
 for(const char*s=name;*s;s++){
    const char *a=s,*b=needle;
    while(*a&&*b&&tolower((unsigned char)*a)==tolower((unsigned char)*b)){a++;b++;}
    if(!*b)return true;
 }
 return false;
}
'''
c=preamble
for name in ['find_table','resolve_item','get_number','field_value','idle_same_channel','idle_visible_above','idle_add_channel','idle_gauge_channels']:
    c+=function(name)+'\n'
c+=r'''
int main(void){
 /* The 3D entry and the 2D entry must use exactly the same ECU Z field. */
 table_count=1;cal_count=1;
 strcpy(tables[0].id,"veTableTbl");strcpy(tables[0].alt,"veTableMap");
 strcpy(tables[0].label,"VE Table");strcpy(tables[0].z,"veTable");
 strcpy(cal[0].name,"veTable");
 resolve_item("veTableTbl",0);
 assert(result_count==1&&results[0].field==0&&results[0].type==1);
 result_count=0;
 resolve_item("veTableMap",0);
 assert(result_count==1&&results[0].field==0&&results[0].type==5);
 /* TunerStudio's veTableDialog3D routes through the 3D map alias. */
 ref_count=1;strcpy(refs[0].owner,"veTableDialog3D");
 strcpy(refs[0].ref,"veTableMap");refs[0].panel=1;
 result_count=0;resolve_item("veTableDialog3D",0);
 assert(result_count==1&&results[0].field==0&&results[0].type==5);

 /* Actual 4x4 U16 calibration. Verify 3D sampling cannot substitute zeros
  * for nonzero bytes and shares exactly the 2D field_value decoder. */
 Field *f=&cal[0];f->type=T_U16;f->kind=K_ARRAY;f->width=f->height=4;
 f->bytes=32;f->validScale=true;f->scale=0.5;f->translate=0;
 uint8_t ecu_bytes[32];
 for(int y=0;y<4;y++)for(int x=0;x<4;x++){
   unsigned raw=50+10*(y*4+x),at=(unsigned)(y*4+x)*2;
   ecu_bytes[at]=(uint8_t)raw;ecu_bytes[at+1]=(uint8_t)(raw>>8);
 }
 double low=INFINITY,high=-INFINITY;
 for(int y=0;y<4;y++)for(int x=0;x<4;x++){
   double z=field_value(ecu_bytes,sizeof ecu_bytes,f,y*f->width+x);
   assert(fabs(z-(25+5*(y*4+x)))<1e-9);
   assert(z>0);if(z<low)low=z;if(z>high)high=z;
 }
 assert(low==25 && high==100);

 /* Home shows configured gauge slots 0-5; unused slots may be duplicated.
  * Ensure fallback never repeats top labels or alias offsets. */
 chan_count=12;selectable_count=12;
 for(int i=0;i<chan_count;i++){
   snprintf(channels[i].name,sizeof(channels[i].name),"extra%d",i);
   channels[i].offset=i*2;channels[i].type=T_U16;
   channels[i].kind=K_SCALAR;channels[i].validScale=true;
   selectable_channels[i]=i;
 }
 for(int i=0;i<9;i++)strcpy(channels[i].name,metric_fields[i]);
 strcpy(channels[9].name,"fuelPressure");
 strcpy(channels[10].name,"intakePressure");
 strcpy(channels[11].name,"oilTemperature");
 for(int i=0;i<9;i++)gauge_fields[i]=i;
 int used[6],n=idle_gauge_channels(used,6,true);
 assert(n>=3);
 for(int i=0;i<n;i++){
   assert(used[i]>=6);for(int j=0;j<i;j++)assert(!idle_same_channel(used[i],used[j]));
 }
 gauge_fields[6]=gauge_fields[0]; /* Duplicate RPM in unused slot */
 channels[10].offset=channels[9].offset; /* Alternative name for same reading */
 n=idle_gauge_channels(used,6,true);
 assert(n>=3);
 for(int i=0;i<n;i++){
   for(int j=0;j<6;j++)assert(!idle_same_channel(used[i],gauge_fields[j]));
   for(int j=0;j<i;j++)assert(!idle_same_channel(used[i],used[j]));
 }
 n=idle_gauge_channels(used,6,false);
 assert(n>=3);
 for(int i=0;i<n;i++){
   assert(used[i]!=0 && used[i]!=2 && used[i]!=4);
   for(int j=0;j<i;j++)assert(!idle_same_channel(used[i],used[j]));
 }
 puts("PASS native: INI 3D/2D alias shares field; nonzero U16 4x4 table; idle gauge dedup");
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='bts39_') as td:
    source=Path(td)/'test.c';exe=Path(td)/'test';source.write_text(c)
    cp=subprocess.run(['gcc','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-misleading-indentation',str(source),'-lm','-o',str(exe)],capture_output=True,text=True)
    assert cp.returncode==0,cp.stderr
    run=subprocess.run([str(exe)],capture_output=True,text=True)
    assert run.returncode==0,run.stderr
    print(run.stdout.strip())
subprocess.run(['gcc','-std=gnu11','-Wno-implicit-function-declaration','-Wno-int-conversion',
 '-fsyntax-only','-I'+str(root/'tests/host_stubs'),'-I'+str(root/'source'),str(root/'source/main.c')],check=True)
print('PASS native 3D and idle selector tests + complete project host C syntax')
