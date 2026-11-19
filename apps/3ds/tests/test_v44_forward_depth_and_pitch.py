#!/usr/bin/env python3
"""v4.4 native stereo-disparity and camera-angle regression.
A host C executable tests actual 3DS screen disparity math and camera extremes.
Also inspects production UI use sites: a visual console test is still required.
"""
from pathlib import Path
import subprocess,tempfile,re
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()

def fun(name):
    match=re.search(r'static\s+(?:void|bool|float)\s+'+name+r'\s*\([^;]*\)\s*\{',s)
    assert match,name
    i=s.index('{',match.start()); depth=0
    for j in range(i,len(s)):
        if s[j]=='{':depth+=1
        elif s[j]=='}':
            depth-=1
            if depth==0:return s[match.start():j+1]
    raise AssertionError(name)

# This is pulled from the source, not a copy of the arithmetic under test.
source='\n'.join(fun(n) for n in ('bts_stereo_shift','bts_front','bts_rear','map3d_project'))
pre='''#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
static float stereo_strength=1.0f;
static int stereo_eye=0;
typedef struct {float x,y,d,ht; unsigned color;bool ok;} Map3dPoint;
'''
post='''int main(void){
 stereo_eye=0;float front_left=bts_front(9),rear_left=bts_rear(4);
 stereo_eye=1;float front_right=bts_front(9),rear_right=bts_rear(4);
 assert(front_right-front_left<-17.9f); /* in front of display */
 assert(rear_right-rear_left>7.9f); /* behind display */
 assert(bts_front(0.8f)<0.f); /* tiny card-to-text bias */
 stereo_strength=0;
 assert(bts_front(16)==0 && bts_rear(4)==0);
 Map3dPoint high,low;
 map3d_project(&high,0,1,.4f,1,0,0,1,0); /* 90deg overhead */
 map3d_project(&low,0,1,.4f,1,0,0,-1,0); /* -90deg underside */
 assert(high.ok && low.ok);
 assert(high.y>108 && low.y<108);
 puts("PASS: moderate front cards, attached text, recessed Tune gauges, overhead/underside camera");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/'depth.c';src.write_text(pre+source+post)
    exe=Path(tmp)/'depth'
    subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror',str(src),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

home=fun('home_digit');rail=fun('draw_tune_gauge_rail');cats=fun('draw_tune_categories_top')
menu=fun('draw_tune_context_top');dialog=fun('draw_tune_dialog_top');map_draw=fun('draw_map_3d')
assert 'bts_front(2.7f)' in home and 'bts_rounded_solid(' in home
assert 'bts_rounded_solid(' in home and 'C2D_TextGetDimensions(&t,value_scale' in home
assert 'bts_rear(2.3f)' in rail and 'bts_front(' not in rail
assert 'active?bts_front(3.4f)' in cats and 'draw_ts_icon(i,x+5,y-1' in cats
assert '.54f' in menu and 'const int rows=8;' in menu and 'float labelpop=cardx-129;' in menu
assert '.54f' in dialog and 'float foreground=boxx-9;' in dialog
assert 'bts_front(3.2f)' in map_draw
assert '#define MAP3D_PITCH_MAX 1.57079632679f' in s
assert '#define MAP3D_PITCH_MIN (-1.57079632679f)' in s
assert s.count('if(map_3d_pitch<MAP3D_PITCH_MIN)')==2
assert s.count('if(map_3d_pitch>MAP3D_PITCH_MAX)')==2
assert 'home_transition' not in s and 'transition_at' not in s
assert 'transition_page' not in s
assert all(x in s for x in ['if(!stopped_rpm())return;', 'if(!arm_ecu())', 'if(!save_backup(pg))return;', 'if(!burn_page(page))'])
print('PASS v4.4 source UI layout, rotation clamps, removed transition and write interlocks')
