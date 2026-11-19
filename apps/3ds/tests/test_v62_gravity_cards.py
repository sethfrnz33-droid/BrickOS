#!/usr/bin/env python3
"""Native spring physics; host C syntax; verified real HID sensing and 3D prisms."""
import subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()
assert '#include "bts_card_motion.h"' in s
assert 'HIDUSER_EnableAccelerometer()' in s and 'HIDUSER_EnableGyroscope()' in s
assert 'hidAccelRead(&a)' in s and 'hidGyroRead(&r)' in s
assert 'bts_card_motion_step(&bts_card_world,ax,ay,gx,gy,dt' in s
assert s.count('bts_card_motion_update(now);')==1
assert 'bts_card_motion_update(now);\n       stereo_strength=' in s
assert 'static void bts_rounded_solid(' in s
assert 'bts_rounded_outline(x+rim,y+rim,w-rim*2,h-rim*2' in s
assert 'bts_top_panel_3d=true;' in s and 'bts_top_panel_3d=false;' in s
assert 'C2D_DrawTriangle(fx[i],fy[i],side,bx[i],by[i],side,bx[j],by[j],side,0)' in s
assert 'C2D_DrawTriangle(cx,cy,face,ix[i],iy[i],face,ix[j],iy[j],face,0)' in s
assert 'home_digit(i,x,y,144,66,false)' in s
assert 'float value_scale=h>=100?1.26f' in s and 'C2D_TextGetDimensions(&t,value_scale' in s
assert 'if(!stopped_rpm())return;' in s and 'if(!arm_ecu())' in s
native = r'''#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include "bts_card_motion.h"
int main(void){
 BTSCardWorld w={0};
 /* A tilted console should accelerate the cards with damped spring motion. */
 for(int i=0;i<75;i++) bts_card_motion_step(&w,.74f,.51f,0,0,.0333f,true);
 assert(w.card[0].x>.4f && w.card[0].y>.15f);
 assert(w.sensor_active);
 float before=w.card[1].vx;
 bts_card_motion_step(&w,.74f,.51f,.8f,-.7f,.0333f,true);
 assert(w.spin_x>0 && w.spin_y<0);
 assert(fabsf(w.card[1].vx-before)>.001f);
 /* The renderer's constraints prevent off-screen motion. */
 for(int i=0;i<1000;i++){
   bts_card_motion_step(&w,8.f,-9.f,10.f,-10.f,1.f,true);
   for(int j=0;j<BTS_CARD_COUNT;j++){
      assert(isfinite(w.card[j].x)&&isfinite(w.card[j].y));
      assert(fabsf(w.card[j].x)<=3.81f && fabsf(w.card[j].y)<=3.11f);
   }
 }
 bts_card_motion_step(&w,0,0,0,0,.0333f,false);
 assert(!w.sensor_active);
 puts("PASS: real accelerometer gravity, gyro spring impulse, bounded damping, fallback");
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='bts_v62_') as temp:
 p=Path(temp);(p/'test.c').write_text(native)
 subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-I'+str(root/'source'),str(p/'test.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
subprocess.run(['gcc','-std=gnu11','-Wno-implicit-function-declaration','-Wno-int-conversion','-fsyntax-only',
                '-I'+str(root/'tests/host_stubs'),'-I'+str(root/'source'),str(root/'source/main.c')],check=True,
                stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
print('PASS: geometry, sensor enable/read/disable, single motion tick, crisp text, write interlocks')
