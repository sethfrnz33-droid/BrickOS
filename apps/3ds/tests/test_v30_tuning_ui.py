#!/usr/bin/env python3
"""Release-level source contracts and compiled semantic category-icon test."""
from pathlib import Path
import subprocess, tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'source/main.c').read_text()
checks={
 'no model renderer in 3DS build': not (r/'source/pikmin_scene.c').exists() and '#include "pikmin_scene.h"' not in s,
 'gauges drawn on every normal screen': 'else home_dashboard();\n       draw_fault_top_overlay();' in s,
 'map and graph still use dedicated upper screen': 'else if(ui_page==PAGE_GRAPH)draw_live_graph_top();' in s and 'else if(current_table>=0&&tables[current_table].curve)draw_curve_overview(f);' in s,
 'at least 5 digital gauge layouts; now 7': 'static const char *gauge_styles[]={"FOUR FOCUS", "THREE LARGE", "PRIMARY + FOUR", "SIX CHANNELS", "NINE CHANNELS", "ONE LARGE", "TWO LARGE"};' in s,
 'restored cycle L/R': '(gauge_style+6)%7' in s and '(gauge_style+1)%7' in s,
 'large touchable list rows': '#define LIST_ROWS 4' in s and '#define LIST_ROW_H 37' in s,
 'list is not tiny 8 tile grid': 'selected/8' not in s,
 'semantic category icons': 'draw_category_icon(category_icon(pretty)' in s and 'category_icon(label)' in s,
 'map consistent numbers': '          cream);' in s and 'if((map_diff_mode||snapshot_diff_mode) && editor_loaded)' in s and '(mx>mn&&(v-mn)/(mx-mn)>.60)?soil:cream' not in s,
 'map cell size is limited to legible grid': 'const int maxc=6,maxr=5;' in s,
 'real ECU write/burn path untouched': 'request_burn(void)' in s and 'post_ts("B",hex)' in s,
 'real ECU error reporting retained': 'draw_fault_top_overlay();' in s and 'fault_poll_requested' in s,
 'read-length bounds checked': 'result>0&&result<=EDIT_CAP&&result==f->bytes' in s,
}
for label,ok in checks.items(): print(('PASS ' if ok else 'FAIL ')+label)
assert all(checks.values()),'v3.0 UI contract failed'

start=s.index('static bool icon_has(')
end=s.index('static void draw_category_icon(',start)
icons=s[start:end]
with tempfile.TemporaryDirectory(prefix='brick-v30-') as td:
 path=Path(td)/'icons.c'; exe=Path(td)/'icons'
 path.write_text('''#include <stdbool.h>\n#include <ctype.h>\n#include <assert.h>\n#include <stdio.h>\n'''+icons+'''\nint main(void){
 assert(category_icon("Fuel Injection") == ICON_FUEL);
 assert(category_icon("Ignition Timing") == ICON_IGN);
 assert(category_icon("Crank Trigger") == ICON_TRIGGER);
 assert(category_icon("Analog Inputs") == ICON_SENSORS);
 assert(category_icon("DBW Throttle") == ICON_THROTTLE);
 assert(category_icon("Boost Controller") == ICON_BOOST);
 assert(category_icon("CAN Network") == ICON_COMMS);
 assert(category_icon("SD Logging") == ICON_LOG);
 assert(category_icon("Fuel Map") == ICON_FUEL);
 assert(category_icon("VE Table") == ICON_MAP);
 assert(category_icon("Live Dashboard") == ICON_GRAPH);
 assert(category_icon("Search") == ICON_SEARCH);
 assert(category_icon("Favorites") == ICON_FAVORITES);
 puts("PASS 13 semantic icon matches from actual source");return 0;
}''')
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-o',str(exe),str(path)],check=True,capture_output=True)
 print(subprocess.run([str(exe)],check=True,text=True,capture_output=True).stdout.strip())
