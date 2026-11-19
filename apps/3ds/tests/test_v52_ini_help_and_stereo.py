#!/usr/bin/env python3
"""v5.2: real-INI context help + comfortable synchronized stereo planes.
This runs selected production helper functions as native C, and checks the
actual UI routing. It does not emulate the 3DS display hardware.
"""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()

def extract(name):
    m=re.search(r'static\s+[\w\s\*]+?\s*'+name+r'\([^;]*\)\s*\{',s)
    assert m, name
    start=s.index('{',m.start());depth=0
    for i in range(start,len(s)):
        if s[i]=='{':depth+=1
        elif s[i]=='}':
            depth-=1
            if depth==0:return s[m.start():i+1]
    raise AssertionError(name)

for tok in ('if(!strcasecmp(section,"SettingContextHelp"))help_store(key,val);',
            'menu_count=cal_count=chan_count=table_count=ref_count=gauge_def_count=ini_graph_count=0;page_ids_count=0;ini_setting_help_count=0;',
            'if(ui_page==PAGE_GROUP||ui_page==PAGE_DETAIL){draw_subfolder_help_bottom();return;}',
            'if(table>=0){', 'tip=help_find(t->z);',
            'const char *help=selected_help_text();',
            'if(help&&*help)draw_ini_help_lines(help);'):
    assert tok in s, tok

subfolder=extract('draw_subfolder_help_bottom')
assert 'tune_reference_strip();' in subfolder
assert 'draw_ini_help_lines(help)' in subfolder
assert 'No description provided by ECU INI' in subfolder
assert 'ui_page==PAGE_GROUP||ui_page==PAGE_DETAIL' in extract('draw_list')
assert 'draw_subfolder_help_bottom()' not in extract('draw_tune_preview_bottom')
assert 'draw_subfolder_help_bottom()' not in extract('draw_tune_categories_top')
assert 'float foreground=boxx-9;' in extract('draw_tune_dialog_top')
assert 'float labelpop=cardx-129;' in extract('draw_tune_context_top')
assert 'float disparity=bts_front(3.2f);' in extract('draw_map_3d')
assert 'stereo_strength*11.5f' not in s

native=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#define INI_HELP_CAP 1024
typedef struct {char key[64];char description[320];} IniSettingHelp;
static IniSettingHelp ini_setting_help[INI_HELP_CAP];
static int ini_setting_help_count=0;
static int stereo_eye=0;
static float stereo_strength=0.80f;
'''
for name in ('copy_text','trim','clean','help_store','help_find','bts_stereo_shift','bts_front','bts_rear'):
    native+='\n'+extract(name)+'\n'
native+=r'''
int main(void){
  help_store("fuelStrategy", "\"How fueling is calculated. <br> Select the ECU strategy.\"");
  assert(ini_setting_help_count==1);
  assert(!strcmp(help_find("FUELSTRATEGY"),"How fueling is calculated. Select the ECU strategy."));
  help_store("veTable", "\"Volumetric efficiency &amp; cylinder charge.\"");
  assert(!strcmp(help_find("vetable"),"Volumetric efficiency & cylinder charge."));
  help_store("fuelStrategy", "\"Updated INI explanation\"");
  assert(ini_setting_help_count==2);
  assert(!strcmp(help_find("fuelstrategy"),"Updated INI explanation"));
  assert(help_find("unavailable") == NULL);
  stereo_eye=0;float near_l=bts_front(3.2f),far_l=bts_rear(1.4f);
  stereo_eye=1;float near_r=bts_front(3.2f),far_r=bts_rear(1.4f);
  assert(near_l>0 && near_r<0 && far_l<0 && far_r>0);
  assert(fabsf(near_l-near_r)<7.0f); /* fuseable within a few pixels */
  stereo_strength=0;
  assert(bts_front(3.2f)==0 && bts_rear(1.4f)==0);
  puts("PASS native INI help decoding, casefold lookup, overwrite, 3D plane sign/spacing");
  return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp)/'v52.c';p.write_text(native)
    exe=Path(tmp)/'v52'
    subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror',str(p),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
subprocess.run(['gcc','-std=gnu11','-Wno-implicit-function-declaration','-Wno-int-conversion',
 '-Wno-incompatible-pointer-types','-fsyntax-only','-I'+str(root/'tests/host_stubs'),
 '-I'+str(root/'source'),str(root/'source/main.c')],check=True)
print('PASS v5.2 source integration and host syntax; 3DS display hardware unverified')
