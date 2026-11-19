#!/usr/bin/env python3
"""Compile the production map projection and test polar stability.

Checks geometry before perspective/fit as well as actual projected nodes from
3D renderer on maps with varying ECU calibration data.
"""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()
match=re.search(r'static void map3d_project\([^;]+?\)\s*\{',s)
assert match
start=match.start(); begin=s.index('{',start);depth=0
for i in range(begin,len(s)):
    depth+=(s[i]=='{')-(s[i]=='}')
    if depth==0:
        projection=s[start:i+1]
        break
assert 'float altitude=(ht-.5f)' in projection
assert 'float polar=sp*sp;' in projection
assert 'float perspective_strength=.24f*(1.f-polar);' in projection
assert 'const float pixels_per_unit=90.f;' in projection
assert 'p->d*disparity' in s[s.index('static void draw_map_3d'):s.index('static void draw_map_overview')]

code=r'''#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
typedef struct {float x,y,d,ht;unsigned color;bool ok;} Map3dPoint;
'''+projection+r'''
static void grid(float pitch,float yaw,float ht,float *spanx,float *spany){
 Map3dPoint pts[9];float ca=cosf(yaw),sa=sinf(yaw),cp=cosf(pitch),sp=sinf(pitch);
 float minx=1e8f,maxx=-1e8f,miny=1e8f,maxy=-1e8f;
 for(int y=0;y<3;y++)for(int x=0;x<3;x++){
    Map3dPoint *p=&pts[y*3+x];map3d_project(p,(float)x-1,(float)y-1,ht,ca,sa,cp,sp,0);
    assert(isfinite(p->x)&&isfinite(p->y));
    if(p->x<minx)minx=p->x;
    if(p->x>maxx)maxx=p->x;
    if(p->y<miny)miny=p->y;
    if(p->y>maxy)maxy=p->y;
 }
 *spanx=maxx-minx;*spany=maxy-miny;
}
int main(void){
 float sx,sy,tx,ty;
 for(int which=0;which<2;which++){
   float pitch=which?-(float)M_PI/2:(float)M_PI/2;
   grid(pitch,0,.2f,&sx,&sy);
   assert(fabsf(sx-sy)<.05f);/* overhead is genuinely square */
   assert(fabsf(sx-180)<.1f);
   grid(pitch,.78539816339f,.9f,&tx,&ty);
   assert(fabsf(tx-ty)<.05f);/* rotating 45deg remains proportionate */
   assert(fabsf(tx-180.f*sqrtf(2.f))<.2f);
 }
 /* Heights centred about the calibration's middle, not the Z=0 floor. */
 Map3dPoint upper,lower;
 map3d_project(&upper,0,0,.9f,1,0,0,1,0);
 map3d_project(&lower,0,0,.1f,1,0,0,1,0);
 assert(fabsf(upper.x-lower.x)<.01f);
 assert(fabsf(upper.y-lower.y)<.01f);
 assert(fabsf(upper.d+lower.d)<.01f);
 puts("PASS: square north/south polar map views, yaw-rotation shape and centred depth");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d)/'pole.c';p.write_text(code)
    exe=Path(d)/'pole'
    cp=subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror',str(p),'-lm','-o',str(exe)],capture_output=True,text=True)
    assert cp.returncode==0,cp.stderr
    cp=subprocess.run([str(exe)],capture_output=True,text=True)
    assert cp.returncode==0,cp.stderr
    print(cp.stdout.strip())
