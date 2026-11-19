#!/usr/bin/env python3
"""v4.5: native bounded burn planner + stereo/legibility contract."""
from pathlib import Path
import subprocess, tempfile, re
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()
header=root/'source/bts_burn_plan.h'
assert header.exists()
checks={
 'stereo menu glyphs share card plane':
   'float foreground=boxx-9;' in s and
   'float labelpop=cardx-129;' in s and
   'bts_rounded_solid(x,y,w,h,7.6f' in s and
   'C2D_TextGetDimensions(&t,value_scale,value_scale,&tw,&th);' in s,
 '2D map focused cell drawn after grid with actual data':
   'if(map_focus==0&&cell>=0&&cell<w*h)' in s and
   'tint=heat_color(field_value(editor,editor_length,f,cell),mn,mx)' in s and
   'float ox=bts_front(4.5f)' in s and
   'stroke_cell(sx+ox,sy-oy,cw,ch,bright)' in s,
 'tiny labels never intentionally drawn at scale <0.43':
   '#define BTS_MIN_TEXT_SCALE .43f' in s and
   'if(sz<BTS_MIN_TEXT_SCALE)sz=BTS_MIN_TEXT_SCALE;' in s and
   'C2D_TextGetDimensions(&t,sz,sz,&tw,&th);' in s,
 'map shows fewer, larger cells':
   'const int maxc=6,maxr=5;' in s and 'float cell_scale=.55f' in s,
 'burn planner used with independent original data':
   'bts_next_burn_span(p->baseline,p->modified,p->length,at,128,12,&start,&len)' in s and
   'if(!ts_write_block(' in s and 'if(!save_backup(pg))return;' in s,
 'ECU burn and RPM interlocks retained':
   'if(!arm_ecu())' in s and 'if(!burn_page(page))' in s and
   'if(!stopped_rpm())return;' in s and 'Post-burn readback mismatch' in s,
}
for name,val in checks.items():
 print(('PASS' if val else 'FAIL'),name)
assert all(checks.values())
source=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bts_burn_plan.h"
static void test(uint8_t *old,uint8_t *new,int n,int expected){
 int at=0,start=0,len=0,count=0;uint8_t mirror[512];memcpy(mirror,old,n);
 while(at<n){
   int next=bts_next_burn_span(old,new,n,at,128,12,&start,&len);
   assert(next>=at && next<=n);
   if(!len)break;
   assert(start>=0&&len>=1&&len<=128&&start+len<=n);
   assert(memcmp(mirror+start,old+start,(size_t)len)==0);
   memcpy(mirror+start,new+start,(size_t)len);
   count++;at=next;
 }
 assert(count==expected);
 assert(memcmp(new,mirror,n)==0);
}
int main(void){
 uint8_t old[512]={0},now[512]={0};
 test(old,now,512,0);
 now[15]=1;now[27]=2;now[50]=3;now[51]=4;
 test(old,now,512,2); /* 12-byte gap merged, larger gap kept split */
 memset(now,0,512);
 for(int i=0;i<512;i+=3)now[i]=1;
 test(old,now,512,4); /* bounded 128-byte chunks */
 memset(now,0,512);now[0]=1;now[511]=1;
 test(old,now,512,2);
 memset(now,0,512);now[127]=1;now[128]=1;
 test(old,now,512,1);
 printf("PASS native verified-byte merging, no-op, span bounds and sparse writes\n");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 src=Path(d)/'burnplan.c';src.write_text(source)
 exe=Path(d)/'burnplan'
 subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(root/'source'),str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
subprocess.run(['gcc','-std=gnu11','-Wno-implicit-function-declaration','-Wno-int-conversion',
 '-Wno-incompatible-pointer-types','-fsyntax-only','-I'+str(root/'tests/host_stubs'),
 '-I'+str(root/'source'),str(root/'source/main.c')],check=True)
print('PASS host C syntax')
