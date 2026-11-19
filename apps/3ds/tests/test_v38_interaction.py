#!/usr/bin/env python3
"""UI v3.8 regression checks; source semantics and native host syntax.
No claims about actual device, hardware graphics or connected ECU.
"""
from pathlib import Path
import subprocess,re
p=Path(__file__).resolve().parents[1];src=(p/'source/main.c').read_text()

def function(name):
    m=re.search(r'static\s+(?:void|bool|int)\s+'+name+r'\([^;]*\)\s*\{',src)
    assert m, name
    start=src.index('{',m.start());depth=0
    for i in range(start,len(src)):
        if src[i]=='{':depth+=1
        elif src[i]=='}':
            depth-=1
            if depth==0:return src[m.start():i+1]
    raise AssertionError(name)

checks={
 'actual rotatable perspective mesh':all(token in function('draw_map_3d') for token in ['C2D_DrawTriangle','qsort(map3d_faces','field_value(editor','cosf(map_3d_yaw','sinf(map_3d_yaw','map3d_nodes']),
 'depth-dependent stereo at maximum slider': 'viewdepth*disparity' in function('map3d_project') and 'float disparity=bts_front(3.2f);' in function('draw_map_3d'),
 '3D view selected by INI, not toggle': 'map_3d_view=!map_3d_view;' not in function('touch_input') and 'is_3d?5:1' in function('resolve_item'),
 '3D circle pad rotates not tunes': 'map_3d_yaw+=(float)cp.dx*.00016f;' in src and 'map_3d_pitch+=(float)cp.dy*.00012f;' in src,
 '3D locks active value editing': 'if(map_3d_view){' in function('handle_keys') and 'if(map_3d_view){' in function('touch_input'),
 '3D never survives opening next field': 'map_3d_view=false;' in function('open_cal'),
 '3 live tuning gauges on top':all(x in function('draw_tune_gauge_rail') for x in ['{0,4,2}','RPM','CLT','AFR','i<3']),
 'tuning groups compressed without smaller fonts': 'float x=7+col*159' in function('draw_tune_categories_top') and 'draw_ts_icon(i,x+5,y-1,.70f)' in function('draw_tune_categories_top') and 'text(x+36,y+5,.53f' in function('draw_tune_categories_top'),
 'larger lower tuning menu font': 'text(34,y+2,.53f' in function('draw_tune_preview_bottom'),
 '10s idle gauges do not lose tree focus': 'input_now-tune_last_activity>=10000' in src and 'draw_tune_idle_gauges_bottom();else draw_tune_preview_bottom()' in function('draw_list'),
 'stable curve Y scale from original ECU data': 'field_value(original' in function('draw_curve_overview') and 'steps[step_index]' not in function('draw_curve_overview'),
 'curve Y labels and conspicuous selected marker':all(x in function('draw_curve_overview') for x in ['high-(high-low)*j/4.0','SELECTED %d/%d','C2D_DrawCircleSolid(marker_x,py,0,10']),
 'fault requires two equal messages': 'fault_candidate_count<2' in function('fault_accept_message') and 'strcmp(message,fault_candidate)' in function('fault_accept_message'),
 'invalid raw error bytes rejected': 'else {valid=false;break;}' in function('net_thread'),
 'all ECU safety checks preserved':all(x in src for x in ['if(!stopped_rpm())return;','if(!arm_ecu())','if(!save_backup(pg))return;','if(!burn_page(page))']),
 'Idle icon is visible after recrop':(p/'art/tsicons/04.png').stat().st_size>400,
 'no standalone map browser': 'All ECU Maps' not in function('draw_tune_categories_top')
}
for name,v in checks.items():print(('PASS ' if v else 'FAIL ')+name)
assert all(checks.values())
subprocess.run(['gcc','-std=gnu11','-Wno-implicit-function-declaration','-Wno-int-conversion',
  '-fsyntax-only','-I'+str(p/'tests/host_stubs'),'-I'+str(p/'source'),str(p/'source/main.c')],check=True)
print('PASS host C syntax check; device build not available in this environment')
