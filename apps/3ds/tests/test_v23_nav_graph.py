#!/usr/bin/env python3
"""Compile real v2.3 navigation functions and verify graph/render contracts."""
from pathlib import Path
import subprocess, tempfile, re
src = (Path(__file__).parent.parent/'source/main.c').read_text()

def extract(signature):
    start=src.index(signature)
    begin=src.index('{',start)
    depth=1;i=begin+1
    while depth:
        if src[i]=='{':depth+=1
        elif src[i]=='}':depth-=1
        i+=1
    return src[start:i]

names=['static int cat_start(int wanted)', 'static int cat_total(void)',
       'static int cat_end(int start)', 'static int count_category_items(void)',
       'static int menu_index(int n)', 'static void nav_capture(void)',
       'static void nav_restore(void)']
funcs='\n'.join(map(extract,names))
# Use the actual extracted functions, not a rewritten reimplementation.
code='''#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
typedef struct {char label[68],target[64];int group;} MenuItem;
#define MENU_CAP 80
#define PAGE_CATS 0
#define PAGE_TRIGGER_SIM 22
#define PAGE_COUNT 23
static MenuItem menus[MENU_CAP];static int menu_count=0;
static int category=0,ui_page=0,selected=0,scroll_y=0;
static struct { int selected,scroll; bool valid; } nav_bookmark[PAGE_COUNT];
''' + funcs + '''
int main(void){
    menus[0].group=0;menus[1].group=3;menus[2].group=1;
    menus[3].group=2;menus[4].group=1;menus[5].group=2;
    menus[6].group=3;menus[7].group=0;menu_count=8;
    int expected[]={1,2,4,6};
    assert(count_category_items()==4);
    for(int i=0;i<4;i++)assert(menu_index(i)==expected[i]);
    assert(menu_index(4)==-1);
    ui_page=1;selected=3;scroll_y=2;nav_capture();
    ui_page=2;selected=0;scroll_y=0;nav_restore();
    assert(selected==0&&scroll_y==0);
    ui_page=1;nav_restore();
    assert(selected==3&&scroll_y==2);
    puts("PASS true INI-order menu, hierarchy, bookmark restore");
    return 0;
}'''
with tempfile.TemporaryDirectory(prefix='brick-v23-') as d:
    c=Path(d)/'test.c'; exe=Path(d)/'test';c.write_text(code)
    subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-o',str(exe),str(c)],check=True, capture_output=True)
    output=subprocess.run([str(exe)],check=True,capture_output=True,text=True)
    print(output.stdout.strip())

assert 'for(int i=0;i<3;i++)live_graph_region(i+graph_bank*3' in src
assert 'int lines=g->count<INI_GRAPH_LINES?g->count:INI_GRAPH_LINES' in src
assert 'draw_category_icon(category_icon(pretty)' in src
assert 'if(ui_page!=old_page)nav_restore()' in src
assert 'if(k&KEY_X)graph_seconds=' in src
assert '(u32)(now-graph_last_sample_ms)<90U' in src
assert 'if((u32)(now-(u32)rate_start_ms)>=2000)' in src
# The 3DS citro2d API takes 8 args, including two separate endpoint colors.
for match in re.finditer(r'C2D_DrawLine\(',src):
    start=match.end();i=start;depth=1;n=1
    while depth:
        c=src[i]
        if c=='(':depth+=1
        elif c==')':depth-=1
        elif c==',' and depth==1:n+=1
        i+=1
    assert n==8, (src[:match.start()].count('\n')+1,n)
print('PASS 9-gauge graph paging, 6 INI traces, semantic icons, 8-arg citro2d calls')
