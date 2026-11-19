#!/usr/bin/env python3
"""v3.7: compile the real INI traversal and focus-transfer implementation.
No hardware, ECU writes, font loader, or 3DS GPU is simulated here.
"""
from pathlib import Path
import re, subprocess, tempfile
p=Path(__file__).resolve().parents[1]
s=(p/'source/main.c').read_text()

def fn(name):
    m=re.search(r'static\s+(?:int|bool|void)\s+'+re.escape(name)+r'\([^;]*\)\s*\{',s)
    assert m,name
    left=s.index('{',m.start());depth=0
    for pos in range(left,len(s)):
        if s[pos]=='{':depth+=1
        elif s[pos]=='}':
            depth-=1
            if depth==0:return s[m.start():pos+1]
    raise ValueError(name)

c='''#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
typedef struct {char label[68],target[64];int group;} MenuItem;
typedef struct {int type,field;char label[72];} Resolved;
typedef struct {int unused;} Field;
static MenuItem menus[70];static int menu_count=0,table_count=0;
static bool menu_loaded=true,tune_lower_focus=false,tune_from_tree=false,map_3d_view=false;
static bool detail_from_maps=false,search_origin=false,favorites_origin=false;
static int tune_bucket=1,tune_lower_selected=0,tune_lower_scroll=0,tune_open_group=-1;
static int ui_page=34,group_item=-1,field_return_page=0,ini_graph_return=0;
static int result_count=0,cal_count=0;
static Resolved results[5];static Field cal[5];
static char opened[70];static int captures=0;
static void note(const char*s){(void)s;}
/* v5.3 optional audio hook: nav-only test deliberately does not play media. */
static bool sfx_enabled=true,event_audio_enabled=true;
static int bts_sound_for_entry(const char *label,const char *target){
 (void)label;(void)target;return -1;
}
static void bts_audio_play(int cue,bool enabled){(void)cue;(void)enabled;}
static void bts_context_named(const char *label,const char *target){
 (void)label;(void)target; /* nav focus test: audio handled separately */
}
static void nav_capture(void){captures++;}
static void open_target(const char*target,const char*label){
 (void)label;strncpy(opened,target,sizeof(opened)-1);ui_page=3; /* PAGE_FIELD */
}
static void open_cal(int id,const char*label){(void)id;(void)label;ui_page=3;}
'''
# Source enum text (declaration matches actual ROOT and FIELD IDs).
assert 'PAGE_TUNE_TREE' in s
c+='\n'+re.search(r'enum \{PAGE_CATS[^;]+;',s).group(0)+'\n'
# The exact, native traversal/focus functions are compiled rather than copied.
for name in ('cat_start','cat_total','cat_end'):
    c+=fn(name)+'\n'
a=s.index('#define TUNE_BUCKET_COUNT 11')
b=s.index('/* rusEFI uses groupMenu/groupChildMenu',a)
c+=s[a:b]+'\n'
c+=fn('tune_enter_preview')+'\n'
c+='''
static void add(int group,const char *name,const char *target){
 MenuItem*m=&menus[menu_count++];m->group=group;
 strncpy(m->label,name,sizeof(m->label)-1);
 if(target)strncpy(m->target,target,sizeof(m->target)-1);
}
int main(void){
 add(0,"Setup",NULL);add(3,"Vehicle Information","vin");
 add(0,"Fuel",NULL);add(3,"VE","veMap");add(1,"Injector Setup",NULL);
 add(2,"Injector Deadtime","deadtime");add(3,"Staged Injection","staged");
 add(0,"Ignition",NULL);add(3,"Ignition Advance","sparkTable");
 add(0,"Experimental ECU",NULL);add(3,"Experimental Table","experimental");
 assert(tune_bucket_of("Fuel")==1);
 assert(tune_bucket_of("Ignition")==2);
 assert(tune_bucket_of("Experimental ECU")==10);
 assert(tune_visible_buckets()==11);
 assert(tune_preview_total(1,-1)==3);
 assert(tune_preview_index(1,-1,0)==3);
 assert(tune_preview_index(1,-1,1)==4);
 assert(tune_preview_index(1,-1,2)==6);
 assert(tune_preview_index(1,4,0)==5);
 assert(tune_preview_total(1,4)==1);
 table_count=250; /* 250 maps cannot generate a synthetic All Maps shortcut */
 assert(tune_preview_total(1,-1)==3);
 assert(tune_preview_total(9,-1)==0);
 assert(tune_section_count(9)==0);
 assert(!tune_lower_focus && ui_page==PAGE_TUNE_TREE);
 tune_enter_preview(); /* top A: just move to lower focus */
 assert(tune_lower_focus&&tune_lower_selected==0&&ui_page==PAGE_TUNE_TREE);
 tune_enter_preview(); /* bottom A: open real VE dialog */
 assert(ui_page==PAGE_FIELD&&field_return_page==PAGE_TUNE_TREE);
 assert(!strcmp(opened,"veMap")&&captures==1);
 ui_page=PAGE_TUNE_TREE;tune_lower_selected=1;
 tune_enter_preview(); /* Injector Setup is a group; stay on same two screens */
 assert(ui_page==PAGE_TUNE_TREE&&tune_open_group==4);
 assert(tune_lower_selected==0);
 tune_enter_preview(); /* injector child opens actual ECU setting */
 assert(!strcmp(opened,"deadtime")&&ui_page==PAGE_FIELD);
 printf("PASS 2-stage focus, real Fuel menu order, nested folder, map-only-through-INI\\n");
 return 0;
}
'''
# remove mocked magic page constants now that enum was inserted after stubs;
# app globals should be initialized using numeric values matching the checked enum.
assert 'PAGE_TUNE_TREE' in c
with tempfile.TemporaryDirectory(prefix='bts37_') as d:
    f=Path(d)/'nav.c';f.write_text(c)
    binary=Path(d)/'nav'
    res=subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror',
                        '-Wno-unused-function','-Wno-unused-variable',str(f),'-o',str(binary)],
                        capture_output=True,text=True)
    assert res.returncode==0,res.stderr
    run=subprocess.run([str(binary)],capture_output=True,text=True)
    assert run.returncode==0,run.stderr
    print(run.stdout.strip())

required={
 'true icon assets present': (p/'source/ts_icons.S').exists() and len(list((p/'art/tsicons').glob('*.png')))==13,
 'correct sprite order': (p/'art/tsicons.t3s').read_text().count('.png')==13,
 'TS icons actually rendered': 'draw_ts_icon(i,x+5,y-1,.70f)' in fn('draw_tune_categories_top'),
 'upper root does not show children': 'tune_preview_index' not in fn('draw_tune_categories_top'),
 'bottom preview shows children immediately': 'tune_preview_index(tune_bucket,tune_open_group,row)' in fn('draw_tune_preview_bottom'),
 'touch bottom selects, then opens': 'if(tune_lower_focus && idx==tune_lower_selected)tune_enter_preview();' in fn('touch_input'),
 'B moves focus back upward': 'else tune_lower_focus=false;' in fn('back'),
 'screen editor shows selected dialog on top': 'draw_tune_dialog_top();return;' in fn('draw_tune_context_top'),
 'scalar editor on top too': 'draw_scalar_top();else if(ui_page==PAGE_TRIGGER_SIM)' in s,
 'no global X maps shortcut': 'else change_page(PAGE_MAPS);return;' not in s,
 'no synthetic maps in View': 'int n=(bucket==9&&table_count>0)?1:0;' not in s,
 'DS Settings only 4 choices': 'case PAGE_SETTINGS_HUB:return 4;' in fn('workspace_count'),
 'DS gauge page no extra log/diagnostic tools': 'if(ui_page==PAGE_DISPLAY)return 10;' in fn('active_count'),
 'A-only power-cycle reminder intact': 'if(k&KEY_A){power_cycle_popup=false;' in fn('handle_keys'),
 'no altered ECU binary write framing': s.count('post_ts("B",hex)')==1,
 'explicit build manifest including atlas': 'export OFILES := main.o font_assets.o ts_icons.o' in (p/'Makefile').read_text(),
}
for name,passed in required.items():print(('PASS ' if passed else 'FAIL ')+name)
assert all(required.values())
subprocess.run(['gcc','-std=gnu11','-Wno-implicit-function-declaration','-fsyntax-only',
                '-I'+str(p/'tests/host_stubs'),'-I'+str(p/'source'),str(p/'source/main.c')],check=True)
print('PASS full-source host C syntax check; actual devkitARM hardware build unverified')
