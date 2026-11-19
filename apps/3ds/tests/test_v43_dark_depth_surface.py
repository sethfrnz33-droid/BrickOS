#!/usr/bin/env python3
"""V4.3 visual contract: no fake map skirts, thin grid, stereo focus,
    unblocked full-height upper views, retained 3D map rendering.

This is source verification only; separate v41 native test executes the exact
3D map renderer and verifies calibrated triangle counts.
"""
from pathlib import Path
import re, subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()

def fun(name):
    matches=list(re.finditer(r'static\s+(?:void|bool|int|float)\s+'+name+r'\s*\([^;]*\)\s*\{',s))
    assert len(matches)==1,(name,len(matches))
    x=matches[0].start(); at=s.index('{',x);d=0
    for n in range(at,len(s)):
        if s[n]=='{':d+=1
        elif s[n]=='}':
            d-=1
            if d==0:return s[x:n+1]
    assert False

map_src=fun('draw_map_3d');grd=fun('draw_depth_grid')
home=fun('home_dashboard');cats=fun('draw_tune_categories_top')
rail=fun('draw_tune_gauge_rail')
checks={
 'no fake zero floor, skirts, or artificial base triangles':
   'map3d_project(&ga' not in map_src and
   'map3d_project(&ground' not in map_src and
   map_src.count('C2D_DrawTriangle(')==1,
 'renderer only draws real loaded ECU fields':
   'field_value(original,editor_length,f,i)' in map_src and
   'field_value(editor,editor_length,f,row*w+col)' in map_src and
   'editor_length!=f->bytes' in map_src,
 'cell highlight is rendered elevated with per-eye parallax':
   'bts_front(3.2f)' in map_src and 'focus->y-raise' in map_src and
   'stereo_strength' in map_src and not __import__('re').search(r'editor\s*\[[^]]+\]\s*=',map_src),
 'perspective background grid is faint and deliberately recessed':
   'C2D_DrawLine' in grd and 'bts_rear(1.4f)' in grd and
   '(int)(18.f+37.f*t)' in grd,
 'homepage retains 6 live ECU gauges and left status rail':
   'for(int i=0;i<6;i++)' in home and 'home_status_rail()' in home,
 'home gauges are in front of grid; Tune gauges deliberately behind menu':
   'bts_rear(2.3f)' in rail and 'round_panel(x+2,y+5' in rail and
   'bts_front(2.7f)' in fun('home_digit') and 'bts_rounded_solid(' in fun('home_digit'),
 'highlighted tuning tile comes forward with text still attached':
   'active?bts_front(3.4f)' in cats and
   'draw_ts_icon(i,x+5,y-1' in cats,
 'no wasted top Tune title bar, enlarged readable labels':
   'rect(0,0,400,23,soil)' not in cats and 'y+5,.53f' in cats,
 'full width title band removed on other upper views':
   'rect(0,0,400,30,soil)' not in fun('draw_tune_context_top') and
   'rect(0,0,400,28,soil)' not in fun('draw_scalar_top'),
 'black neutral palette with tinted-but-readable labels':
   'bg=C2D_Color32(2,4,7,255)' in s and 'secondary=C2D_Color32(172,188,202,255)' in s,
 'no screen transitions':
   'home_transition' not in s and 'transition_at' not in s and
   'transition_page' not in s,
 'burn and power cycle interlocks preserved':
   all(v in s for v in ('if(!stopped_rpm())return;','if(!arm_ecu())','if(!save_backup(pg))return;','if(!burn_page(page))')),
}
for k,v in checks.items():print(('PASS ' if v else 'FAIL ')+k)
assert all(checks.values())
subprocess.run(['gcc','-std=gnu11','-Wno-implicit-function-declaration','-Wno-int-conversion','-fsyntax-only',
    '-I'+str(root/'tests/host_stubs'),'-I'+str(root/'source'),str(root/'source/main.c')],check=True)
print('PASS host C syntax; devkitARM/actual stereoscopic rendering not verified')
