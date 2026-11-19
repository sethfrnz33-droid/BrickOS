#!/usr/bin/env python3
"""Verify the three newly supplied clips and event wiring, including real Undo code."""
from pathlib import Path
import re, subprocess, tempfile
import numpy as np
root=Path(__file__).resolve().parents[1]
main=(root/'source/main.c').read_text()
rules=(root/'source/bts_audio_rules.h').read_text()
audio=(root/'source/bts_event_audio.h').read_text()
assert 'BTS_SOUND_LAMBDA_DISPLAY' in rules
assert 'BTS_SOUND_UNDO' in rules
assert 'BTS_SOUND_CYLINDER_OVER6' in rules
assert 'if(direction<0){' in main and 'bts_audio_play(BTS_SOUND_UNDO,event_audio_enabled)' in main
assert 'bts_sound_for_change(f->name,"",old,value)' in main
assert 'bts_sound_for_change(f->name,choices[selected].label,old_choice,(double)chosen)' in main
assert '"Shampoo Is Betta (Lambda Display)"' in main
assert '"KidPix I Made a Boo Boo (Undo)"' in main
assert '"Alex Jones Dracula (Cylinders > 6)"' in main
assert 'bts_embedded_13_end' in audio
for i in (11,12,13):
    a=np.fromfile(root/'art/event_audio'/f'{i:02}.pcm',dtype='<i2').astype('float64')/32768
    assert len(a)>22050
    assert max(abs(a))<=.94
    assert .16<np.sqrt(np.mean(a**2))<.38

native=r'''
#include <stdio.h>
#include <assert.h>
#include "bts_audio_rules.h"
int main(void){
  assert(BTS_SOUND_COUNT==14);
  assert(bts_sound_for_entry("Lambda Display Settings","lambdaDisplayDialog")==BTS_SOUND_LAMBDA_DISPLAY);
  assert(bts_sound_for_entry("Wideband Lambda Display","someDialog")==BTS_SOUND_LAMBDA_DISPLAY);
  assert(bts_sound_for_entry("Lambda target table","lambdaTarget")==BTS_SOUND_LAMBDA_DISPLAY);
  assert(bts_sound_for_change("cylindersCount","",6,8)==BTS_SOUND_CYLINDER_OVER6);
  assert(bts_sound_for_change("cylindersCount","",4,7)==BTS_SOUND_CYLINDER_OVER6);
  assert(bts_sound_for_change("cylindersCount","",7,8)==BTS_SOUND_CYLINDER_OVER6);
  assert(bts_sound_for_change("cylindersCount","",6,6)==-1);
  assert(bts_sound_for_change("cylindersCount","",8,4)==BTS_SOUND_GONG);
  assert(bts_sound_for_change("cylindersCount","",5,4)==BTS_SOUND_GONG);
  assert(bts_sound_for_change("triggerType","Honda K20",0,4)==BTS_SOUND_GONG);
  assert(bts_sound_for_change("someSetting","",6,8)==-1);
  puts("PASS: word-matched Lambda and Cylinder interactions");
  return 0;
}
'''
# Exercise the actual edit_history_move function in a minimal C harness.
start=main.index('static void edit_history_move(int direction){')
end=main.index('static void open_cal(',start)
func=main[start:end]
undo_test=r'''
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
enum{BTS_SOUND_UNDO=12};
#define EDIT_CAP 1024
#define EDIT_UNDO_CAP 16
typedef struct {uint8_t z[EDIT_CAP],axis[2][512];int zlen,alen[2];} EditSnapshot;
static EditSnapshot edit_undo[EDIT_UNDO_CAP];
static int edit_undo_count=0,edit_undo_at=-1;
static uint8_t editor[EDIT_CAP],axis_data[2][512];
static int editor_length=4,axis_len[2]={0,0};
static bool editor_loaded=true,staged=true,axis_ready[2]={false,false};
static bool map_text_dirty=false,confirm_write=false,event_audio_enabled=true;
static int played=0,last_cue=-1;
static void note(const char*msg){(void)msg;}
static bool bts_audio_play(int cue,bool enabled){if(enabled){played++;last_cue=cue;}return enabled;}
'''+func+r'''
int main(void){
 edit_undo_count=2;edit_undo_at=1;
 edit_undo[0].zlen=edit_undo[1].zlen=4;
 edit_undo[0].z[0]=10;edit_undo[1].z[0]=20;editor[0]=20;
 edit_history_move(-1);
 assert(played==1&&last_cue==BTS_SOUND_UNDO&&editor[0]==10);
 edit_history_move(-1);
 assert(played==1); /* No history -> no sound */
 edit_history_move(1);
 assert(played==1&&editor[0]==20); /* Redo silent */
 staged=false;edit_history_move(-1);
 assert(played==1); /* Blocked undo silent */
 puts("PASS: actual Undo implementation plays KidPix exactly on successful undo");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    td=Path(td)
    for label,src,include in [('rules',native,True),('undo',undo_test,False)]:
        f=td/f'{label}.c';exe=td/label;f.write_text(src)
        args=['gcc','-std=gnu11','-Wall','-Wextra','-Werror']
        args+=['-Wno-unused-function']
        args+=['-I'+str(root/'source'),str(f),'-o',str(exe)]
        p=subprocess.run(args,capture_output=True,text=True)
        assert p.returncode==0,p.stderr
        subprocess.run([str(exe)],check=True)
print('PASS: 14 assets, 3 actual uploads, soundboard, direct numeric and choice events')
