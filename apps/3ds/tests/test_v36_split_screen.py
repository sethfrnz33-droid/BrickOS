#!/usr/bin/env python3
"""v3.6 regression: source-level UI routing and independent compile-time taxonomy checks.
Requires no ECU and no 3DS cross compiler; hardware rendering remains unverified.
"""
from pathlib import Path
import subprocess, tempfile, re
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()

def function(name):
    m=re.search(r'static\s+(?:(?:int|bool|void)\s+|const\s+char\s*\*\s*)'+name+r'\([^;]*\)\s*\{',s)
    assert m,name
    a=m.start();depth=0;start=s.index('{',a)
    for end in range(start,len(s)):
        if s[end]=='{': depth+=1
        elif s[end]=='}':
            depth-=1
            if depth==0:return s[a:end+1]
    raise AssertionError('unterminated '+name)

checks={
 'four-mode launcher exactly matches user request': 'static const char* root[]={"Tune","Analyze","Logging","DS Settings"};' in s and 'case PAGE_HUB:return 4;' in s,
 'six real gauges on upper HOME': 'for(int i=0;i<6;i++)' in function('home_dashboard') and 'home_digit(i,x,y,144,66,false);' in function('home_dashboard'),
 'status rail contains wifi, PTMU battery, real update frequency': all(t in function('home_status_rail') for t in ('wifi_candidate','battery_known','battery_level','actual_hz','%.1f Hz')),
 'PTMU ordinal 0..5 not percentage': 'PTMU_GetBatteryLevel(&level)' in function('update_system_battery') and 'level<=5' in function('update_system_battery'),
 'actual battery API lifetime': all(x in s for x in ('ptmuInit()','ptmuExit()','update_system_battery(now)')),
 'all top tabs screenshot matched': '"Setup", "Fuel", "Ignition", "Cranking", "Idle", "Advanced"' in s and '"Sensors", "CAN-bus", "Controller", "View", "Other ECU Pages"' in s,
 'root tune previews real INI entries on bottom': 'tune_preview_index(tune_bucket,tune_open_group,row)' in function('draw_tune_preview_bottom'),
 'A transfers cursor from category to bottom': 'tune_lower_focus=true;tune_lower_selected=tune_lower_scroll=0;' in function('tune_enter_preview'),
 'top tree contains only ten base tabs': 'draw_tune_categories_top();return;' in function('draw_tune_context_top') and 'tune_bucket_names[i]' in function('draw_tune_categories_top'),
 'lower tune panel shows submenus; reference gauges moved to top': 'draw_tune_preview_bottom();return;' in function('draw_list') or ('draw_tune_idle_gauges_bottom();else draw_tune_preview_bottom();return;' in function('draw_list') and 'tune_lower_focus' in function('draw_tune_preview_bottom') and 'draw_tune_gauge_rail();' in function('draw_tune_categories_top')),
 'existing ECU write safety preserved': all(x in s for x in ('if(!stopped_rpm())return;', 'if(!arm_ecu())','if(!save_backup(pg))return;','if(!burn_page(page))')),
 'power cycle reminder remains explicit A acknowledgment': 'if(k&KEY_A){power_cycle_popup=false;' in function('handle_keys'),
 'stray game code cannot relink': 'export OFILES := main.o font_assets.o ts_icons.o' in (root/'Makefile').read_text(),
 'root launch touch uses new 2x2 cards': 'int index=row*2+col;selected=index;workspace_enter(PAGE_HUB,index);' in function('touch_input'),
}
for name,ok in checks.items():print(('PASS' if ok else 'FAIL'),name)
assert all(checks.values())

# Compile the real navigation taxonomy with synthetic INI menu headings.
a=s.index('#define TUNE_BUCKET_COUNT 11')
b=s.index('/* rusEFI uses groupMenu/groupChildMenu',a)
catstart=function('cat_start')
cattotal=function('cat_total')
code='''#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
typedef struct {char label[68],target[64];int group;} MenuItem;
static MenuItem menus[80];static int menu_count=0,table_count=0;
static bool tune_lower_focus=false,tune_from_tree=false;
static int tune_lower_selected=0,tune_lower_scroll=0,tune_open_group=-1;
'''+catstart+'\n'+cattotal+'\n'+s[a:b]+'''
int main(void){
 const char *tabs[]={"Setup","Fuel","Ignition","Cranking","Idle","Advanced","Sensors","CAN-bus","Controller","View","Other ECUs"};
 for(int i=0;i<11;i++){
    MenuItem*m=&menus[menu_count++];strncpy(m->label,tabs[i],sizeof(m->label)-1);m->group=0;
 }
 assert(cat_total()==11);
 assert(!strcmp(tune_bucket_names[0],"Setup"));
 assert(!strcmp(tune_bucket_names[10],"Other ECU Pages"));
 for(int i=0;i<11;i++){
    assert(tune_bucket_of(tabs[i])==i);
    assert(tune_section_count(i)==1);
    assert(tune_section_at(i,0)==i);
 }
 assert(tune_bucket_of("Sensors")==6);
 assert(tune_bucket_of("Fuel")==1);
 assert(tune_bucket_of("&CAN-bus")==7);
 assert(tune_bucket_of("Completely unknown tab")==10);
 table_count=50;
 assert(tune_section_count(9)==1 && tune_section_at(9,0)==9);
 assert(tune_preview_total(9,-1)==0);
 assert(tune_visible_buckets()==10);
 puts("PASS compiled tab mapping + no standalone all-maps entry");
}
'''
with tempfile.TemporaryDirectory(prefix='bts_v36_') as t:
    td=Path(t);(td/'test.c').write_text(code)
    subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-function',str(td/'test.c'),'-o',str(td/'test')],check=True)
    subprocess.run([str(td/'test')],check=True)
subprocess.run(['gcc','-std=gnu11','-Wno-implicit-function-declaration','-fsyntax-only',
                '-I'+str(root/'tests/host_stubs'),'-I'+str(root/'source'),str(root/'source/main.c')],check=True)
print('PASS full main.c host syntax; real 3DS compilation remains to be verified')
