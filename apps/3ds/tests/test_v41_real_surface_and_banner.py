#!/usr/bin/env python3
"""Native execution of exact 3D surface renderer with real numeric calibration bytes.
Host-only: cannot validate 3DS depth state, raster output or ECU transport.
"""
from pathlib import Path
import subprocess, tempfile, re
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()
start=s.index('/* Real ECU-calibration perspective surface:')
end=s.index('static void draw_map_overview(Field *f){',start)
source=s[start:end]
assert 'fmin(lo,f->low)' not in source and 'fmax(hi,f->high)' not in source
assert 'return C2D_Color32(r,g,b,255);' in source
assert 'viewdepth*disparity' in source
assert 'FLAT ECU CALIBRATION' in source
assert 'map3d_heat(heat,light)' in source
assert 'MAP3D_TRI_CAP' in source
assert 'ADDITIONAL ECU GAUGES' not in s
assert 'LIVE ECU / EXTRA GAUGES' not in s
assert 'bts_logo_sheet' not in s
assert (root/'home-banner/e46_home_banner.glb').exists()
assert 'makebanner -ci home-banner/banner.cgfx' in (root/'build-cia.sh').read_text()
assert 'extendedbanner' in (root/'build-cia.sh').read_text()

pre=r'''
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint32_t u32;
typedef struct { int width,height,bytes,type; char unit[20]; } Field;
static unsigned char original[2048], editor[2048];
static int editor_length=0,cell=0;
static float map_3d_yaw=-.64f,map_3d_pitch=.80f,stereo_strength=0;
static int stereo_eye=0;
static const char *open_title="VE";
static u32 bg=1,soil=2,cream=3,orange=4,white=5,secondary=6,bright=7,outline=8;
static int triangles=0,face_colors=0;
static float ymin=10000,ymax=-10000;
static int displayed_flat=0;
static u32 colors[2000];static int ncolors=0;
static u32 C2D_Color32(int r,int g,int b,int a){return ((u32)a<<24)|((u32)b<<16)|((u32)g<<8)|r;}
static int dsize(int t){(void)t;return 2;}
static double field_value(const uint8_t *bytes,int length,const Field *f,int i){
 if(i<0||i>=f->width*f->height||i*2+1>=length)return NAN;
 return (double)(bytes[i*2] | (bytes[i*2+1]<<8));
}
static void draw_depth_grid(float w,float h){(void)w;(void)h;}
static float bts_stereo_shift(float px){return (stereo_eye?1.f:-1.f)*stereo_strength*px;}
static float bts_front(float px){return -bts_stereo_shift(px);}
static void rect(float x,float y,float w,float h,u32 col){(void)x;(void)y;(void)w;(void)h;(void)col;}
static void text(float x,float y,float scale,u32 col,const char*s){
 (void)x;(void)y;(void)scale;(void)col;if(strstr(s,"FLAT ECU CALIBRATION"))displayed_flat++;
}
static void C2D_DrawTriangle(float ax,float ay,u32 ac,float bx,float by,u32 bc,float cx,float cy,u32 cc,float z){
 (void)ax;(void)bx;(void)cx;(void)bc;(void)cc;(void)z;
 triangles++;if(ay<ymin)ymin=ay;if(by<ymin)ymin=by;if(cy<ymin)ymin=cy;
 if(ay>ymax)ymax=ay;if(by>ymax)ymax=by;if(cy>ymax)ymax=cy;
 bool seen=false;for(int i=0;i<ncolors;i++)if(colors[i]==ac)seen=true;
 if(!seen&&ncolors<2000)colors[ncolors++]=ac;
}
static void C2D_DrawLine(float x,float y,u32 a,float x2,float y2,u32 b,float z,float w){
 (void)x;(void)y;(void)a;(void)x2;(void)y2;(void)b;(void)z;(void)w;
}
static void C2D_DrawCircleSolid(float x,float y,float z,float r,u32 c){
 (void)x;(void)y;(void)z;(void)r;(void)c;
}
'''
post=r'''
int main(void){
 Field f={4,4,32,0,"%"}; editor_length=32;cell=7;
 for(int i=0;i<16;i++){
   unsigned v=100+5*i;
   editor[i*2]=original[i*2]=(unsigned char)v;
   editor[i*2+1]=original[i*2+1]=(unsigned char)(v>>8);
 }
 draw_map_3d(&f);
 assert(triangles==18);assert(ncolors>=5); /* Exactly 3x3 cells x 2; no artificial zero-floor walls */
 assert(ymax-ymin>55);
 assert(map3d_nodes[0].ht==0 && map3d_nodes[15].ht==1);
 stereo_strength=1;stereo_eye=0;draw_map_3d(&f);
 float eye_left=map3d_nodes[15].x;
 stereo_eye=1;draw_map_3d(&f);
 assert(fabs(map3d_nodes[15].x-eye_left)>2.0f); /* moderate, fuseable stereo separation */
 /* INI limits are deliberately absent from the renderer; actual data wins. */
 assert(map3d_normalize(130,100,175)>.3f);
 for(int i=0;i<16;i++){original[i*2]=editor[i*2]=5; original[i*2+1]=editor[i*2+1]=0;}
 displayed_flat=0;draw_map_3d(&f);
 assert(displayed_flat==1);
 puts("PASS: real U16 height contrast, per-cell colours, stereoscopic parallax, constant-table warning");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    path=Path(d)/'test.c';path.write_text(pre+source+post)
    exe=Path(d)/'test'
    cp=subprocess.run(['gcc','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-variable','-Wno-misleading-indentation',str(path),'-lm','-o',str(exe)],capture_output=True,text=True)
    assert cp.returncode==0,cp.stderr
    run=subprocess.run([str(exe)],capture_output=True,text=True)
    assert run.returncode==0,run.stderr
    print(run.stdout.strip())
subprocess.run(['gcc','-std=gnu11','-Wno-implicit-function-declaration','-Wno-int-conversion','-fsyntax-only',
    '-I'+str(root/'tests/host_stubs'),'-I'+str(root/'source'),str(root/'source/main.c')],check=True)
print('PASS: full host C syntax; actual 3DS compilation and 3D banner conversion unverified')
