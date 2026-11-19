#!/usr/bin/env python3
"""v3.4: native workspace menu test plus actual source/control wiring contracts.
Does not claim console rendering or ECU validation.
"""
from pathlib import Path
import subprocess,tempfile,re
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()

def grab_fn(name):
    m=re.search(r'static\s+(?:bool|int|const char\*)\s*'+name+r'\([^;]*\)\s*\{',s)
    assert m, name
    start=m.start(); depth=0
    for p in range(s.find('{',start),len(s)):
        if s[p]=='{':depth+=1
        if s[p]=='}':
            depth-=1
            if depth==0:return s[start:p+1]
    raise AssertionError(name)
enum=re.search(r'enum \{PAGE_CATS[^;]+;',s).group(0)
pure='\n'.join(grab_fn(n) for n in ['workspace_page','workspace_count','workspace_title','workspace_action'])
c='''#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
static bool sfx_enabled=true;
'''+enum+'\n'+pure+'''
int main(void){
 assert(workspace_count(PAGE_HUB)==4);
 assert(!strcmp(workspace_title(PAGE_HUB),"BRICK TUNER SUITE"));
 assert(workspace_count(PAGE_TUNE_HUB)==4);
 assert(workspace_count(PAGE_LOGS_HUB)==5);
 assert(workspace_count(PAGE_ANALYZE_HUB)==5);
 assert(workspace_count(PAGE_SETTINGS_HUB)==4);
 assert(!strcmp(workspace_action(PAGE_HUB,0),"Tune"));
 assert(!strcmp(workspace_action(PAGE_HUB,1),"Analyze"));
 assert(!strcmp(workspace_action(PAGE_HUB,2),"Logging"));
 assert(!strcmp(workspace_action(PAGE_HUB,3),"DS Settings"));
 assert(!strcmp(workspace_action(PAGE_ANALYZE_HUB,1),"Compare Two Logs"));
 assert(!strcmp(workspace_action(PAGE_ANALYZE_HUB,2),"Trace Log to Map"));
 assert(!strcmp(workspace_action(PAGE_LOGS_HUB,1),"Browse Saved Logs"));
 assert(!strcmp(workspace_action(PAGE_SETTINGS_HUB,1),"Wi-Fi & ECU Connection"));
 for(int p=PAGE_HUB;p<=PAGE_SETTINGS_HUB;p++){
    assert(workspace_page(p));
    for(int i=0;i<workspace_count(p);i++)assert(*workspace_action(p,i));
    assert(!*workspace_action(p,workspace_count(p)));
 }
 puts("PASS workspace menu labels, ordering and bounds");
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='bts-v34-') as td:
    t=Path(td);(t/'test.c').write_text(c)
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror',str(t/'test.c'),'-o',str(t/'test')],check=True)
    subprocess.run([str(t/'test')],check=True)
checks={
 'launcher is the boot page':'static int ui_page=PAGE_HUB' in s,
 'INI download does not throw user back to category list':'ini_refresh=false; if(ui_page==PAGE_CATS && !menu_loaded)ui_page=PAGE_HUB;' in s,
 'main four modes directly routed':'if(page==PAGE_HUB)' in s and 'case 3:ui_page=PAGE_SETTINGS_HUB;' in s,
 'tune workspace routes only through categories':'ui_page=PAGE_TUNE_TREE;tune_reset_preview()' in s and 'case 1:maps_return_page=PAGE_TUNE_HUB;ui_page=PAGE_MAPS;' not in s,
 'logs workspace has real files':'refresh_log_files();ui_page=PAGE_LOG_BROWSER;' in s,
 'analysis workspace has real functions':all(v in s for v in ['case 0:analysis_return_page=page;insights_open();','case 1:compare_return_page=page;compare_open_picker();','case 2:trace_return_page=page;replay_open_trace();','case 3:quality_return_page=page;quality_open();']),
 'DS settings excludes tuning duplicates':'case 0:display_parent=page;ui_page=PAGE_DISPLAY;' in s and 'case 1:ui_page=PAGE_DIAG;' in s and 'case 0:cats_return_page=page;ui_page=PAGE_CATS;' not in s,
 'replay touch actions':'if(tp.py>=120&&tp.py<194&&tp.px>=7&&tp.px<312)' in s and 'replay_action(row*3+col);' in s,
 'replay visibly labels 6 operations':'"PAUSE":"PLAY","COMPARE","ANALYZE","TRACE MAP","EXPORT","QUALITY"' in s,
 'map touch undo / burn without duplicate 3D toggle':'map_3d_view=!map_3d_view;' not in s and 'edit_history_move(-1);' in s and 'confirm_write=true;note("BURN REVIEW:' in s,
 'map tools still local until write':'All operations local until explicit BURN' in s,
 'map writes continue via guarded request':'static void request_burn(void)' in s and 'if(confirm_write)request_burn();' in s,
 'dashboard touchscreen actions':'int index=row*2+col;' in s and 'gauge_stats_reset(i)' in s and 'display_parent=PAGE_DASH' in s,
 'global SELECT quick jump to home':'ui_page=PAGE_HUB;selected=scroll_y=0;sfx_play(1);return;' in s,
 'back bookmarks used':'if(ui_page!=old_page)nav_restore();' in s,
 'no new ECU protocol commands':s.count('post_ts("B",hex)')==1,
}
for desc,ok in checks.items():print(('PASS' if ok else 'FAIL'),desc)
assert all(checks.values())
