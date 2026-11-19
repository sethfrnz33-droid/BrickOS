#!/usr/bin/env python3
"""v3.5 host suite: verify INI category coverage and edit-interaction contracts.
   No physical ECU or real devkitARM compiler is present in this test environment.
"""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()

def fun(name):
    import re
    m=re.search(r'static (?:bool|int) '+name+r'\([^;]*\)\s*\{',s)
    assert m,name
    a=m.start(); brace=s.index('{',a);level=0
    for i in range(brace,len(s)):
        if s[i]=='{': level+=1
        elif s[i]=='}':
            level-=1
            if level==0:return s[a:i+1]
    raise AssertionError(name)

c='''#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdbool.h>
typedef struct {char label[68],target[64];int group;} MenuItem;
static MenuItem menus[80];
static int menu_count=0,table_count=0;
static bool tune_lower_focus=false,tune_from_tree=false;
static int tune_lower_selected=0,tune_lower_scroll=0,tune_open_group=-1;
'''
for name in ['cat_start','cat_total']:
    c+=fun(name)+'\n'
a=s.index('#define TUNE_BUCKET_COUNT')
b=s.index('/* rusEFI uses groupMenu/groupChildMenu',a)
c+=s[a:b]
c+='''
static void add(const char *n){MenuItem *m=&menus[menu_count++];
  strncpy(m->label,n,sizeof(m->label)-1);m->group=0;}
int main(void){
 const char *tabs[]={"Setup","Fuel","Ignition","Cranking","Idle", "Advanced",
 "Sensors","CAN-bus","Controller","View","Experimental Feature"};
 for(int i=0;i<11;i++)add(tabs[i]);
 assert(cat_total()==11);
 assert(TUNE_BUCKET_COUNT==11);
 assert(tune_bucket_names[0][0]=='S');
 for(int i=0;i<11;i++){
   assert(tune_bucket_of(menus[i].label)==i);
   assert(tune_section_count(i)==1);
   assert(tune_section_at(i,0)==i);
   assert(tune_section_at(i,1)==-1);
 }
 table_count=8;
 assert(tune_section_count(9)==1);
 assert(tune_section_at(9,0)==9);
 assert(tune_section_at(9,1)==-1);
 assert(tune_preview_total(9,-1)==0); /* unreferenced maps do NOT leak into View */
 assert(tune_visible_buckets()==10);
 puts("PASS 10 TunerStudio tabs and no synthetic map browser");
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='bts35_') as t:
    out=Path(t);(out/'t.c').write_text(c)
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function',str(out/'t.c'),'-o',str(out/'t')],check=True)
    subprocess.run([str(out/'t')],check=True)

checks={
'upper tune screen draws only base tabs': 'draw_tune_categories_top(void)' in s and 'if(ui_page==PAGE_TUNE_TREE){draw_tune_categories_top();return;}' in s,
'bottom preview is INI-driven': 'static int tune_preview_index(int bucket,int group,int nth)' in s and 'draw_tune_preview_bottom(void)' in s,
'A moves focus to bottom without entering another page': 'if(!tune_lower_focus)' in s and 'tune_lower_focus=true;tune_lower_selected=tune_lower_scroll=0;' in s,
'no INI section silently hidden': 'return 10;' in fun('tune_bucket_of'),
'fine adjustment supports 0.001 and 0.01': 'steps[]={0.001,0.01,0.1,1.0,10.0,100.0}' in s,
'direct numeric keyboard validates finite values': 'numeric_edit_prompt(void)' in s and '*end||!isfinite(value)' in s,
'no direct value commits to ECU': 'numeric_edit_prompt' in s and 'note("Exact numeric value staged locally' in s,
'Y increase is independent of fault dialog': 'if(fault_active && (k&KEY_Y) && (hidKeysHeld()&KEY_SELECT)' in s,
'non modal compact fault strip': 'rect(0,214,400,26,berry)' in s,
'fault repair links and browse fallback': 'Browse related ECU tuning category' in s and 'fault_add_menu_hint(' in s,
'power cycle reminder only after verified burn': 'if(power_cycle_needed){\n   power_cycle_popup=true;' in s and 'write_verified=true;' in s,
'power cycle only ack by A': 'if(power_cycle_popup){\n   if(k&KEY_A){power_cycle_popup=false;' in s,
'curve one-dimensional sequential cursor': 'if(current_table>=0&&tables[current_table].curve){\n    int count=w*h;' in s,
'New3DS C stick navigation': 'KEY_CSTICK_RIGHT' in s and 'KEY_CSTICK_DOWN' in s,
'write path still guarded': all(k in s for k in ('if(!stopped_rpm())return;', 'if(!arm_ecu())', 'if(!save_backup(pg))return;','if(!burn_page(page))')),
'explicit icon atlas in source manifest': 'export OFILES := main.o font_assets.o ts_icons.o' in (root/'Makefile').read_text(),
}
for title,ok in checks.items():
    print(('PASS' if ok else 'FAIL'),title)
assert all(checks.values())
subprocess.run(['gcc','-std=gnu11','-Wno-implicit-function-declaration','-fsyntax-only',
                '-I'+str(root/'tests/host_stubs'),'-I'+str(root/'source'),str(root/'source/main.c')],check=True)
print('PASS host C syntax check (device compilation still needed)')
