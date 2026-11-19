#!/usr/bin/env python3
"""Compile actual v3.1 trace, undo, and SD bookmark C functions with tiny test scaffolding."""
from pathlib import Path
import subprocess, tempfile
source=(Path(__file__).resolve().parents[1]/'source/main.c').read_text()

def grab(signature):
    start=source.index(signature)
    begin=source.index('{',start)
    i=begin+1;depth=1
    while depth:
        if source[i]=='{':depth+=1
        elif source[i]=='}':depth-=1
        i+=1
    return source[start:i]

funcs=[
 'static int trace_log_channel(const char *name)',
 'static int trace_table_channels(int table,int *outx,int *outy)',
 'static void trace_coverage_tick(Field *f)',
 'static void edit_history_capture(EditSnapshot *s)',
 'static void edit_history_changed(void)',
 'static void edit_history_move(int direction)',
 'static bool log_basename_valid(const char *name)',
 'static bool log_marks_path(char *out,size_t cap)',
 'static void replay_marks_load(void)',
 'static bool replay_marks_save(void)',
 'static void replay_mark_toggle(void)',
 'static void replay_mark_jump(int direction)',
]
C=r'''
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <math.h>
#include <sys/stat.h>
#include <limits.h>
typedef uint32_t u32;
#define LOG_FIELDS 9
#define TABLE_CAP 350
#define EDIT_CAP 4096
#define EDIT_UNDO_CAP 16
#define LOG_MARK_CAP 64
#define FIELD_CAP 16
#define LOG_MAX_SAMPLES 5
typedef struct{char id[64],alt[64],label[76],x[60],y[60],z[60],xchan[60],ychan[60];bool curve,xro,yro;} TableDef;
typedef struct{int width,height;} Field;
typedef struct{u32 t_ms;float values[9];unsigned valid;} LogSample;
typedef struct{uint8_t z[EDIT_CAP],axis[2][512];int zlen,alen[2];} EditSnapshot;
static Field cal[FIELD_CAP];static TableDef tables[TABLE_CAP];static int table_count;
static char log_channel_names[LOG_FIELDS][64];
static uint16_t trace_coverage[EDIT_CAP];
static int trace_coverage_at=0,trace_valid_samples=0,trace_coverage_peak=0;
static bool replay_trace_active=true,trace_show_coverage=true,trace_coverage_done=false;
static bool axis_ready[2]={true,false};
static int trace_log_x=0,trace_log_y=1;
static int axis_location(int axis,int n,double target,float*pos,int*nearest){
 int p=(int)(target/100.0);if(p<0)p=0;if(p>=n)p=n-1;
 *nearest=p;*pos=(float)p;return axis>=0&&axis<2;
}

static int field_index(const char *s){return strcmp(s,"fuelTable")==0?0:-1;}
static uint8_t editor[EDIT_CAP],axis_data[2][512];static int editor_length=4,axis_len[2]={4,0};
static bool editor_loaded=true,staged=true,map_text_dirty=false,confirm_write=true;
static EditSnapshot edit_undo[EDIT_UNDO_CAP];static int edit_undo_at=-1,edit_undo_count=0;
static EditSnapshot edit_undo_before;
static char note_message[180];static void note(const char*s){snprintf(note_message,sizeof(note_message),"%s",s);}
enum{BTS_SOUND_UNDO=12};
static bool event_audio_enabled=true;
static unsigned undo_audio_count=0;
static bool bts_audio_play(int cue,bool enabled){if(cue==BTS_SOUND_UNDO&&enabled)undo_audio_count++;return enabled;}
static LogSample replay_buffer[LOG_MAX_SAMPLES];static int replay_count=4;
static char replay_filename[48]="bricktuner-0001.csv";
static u32 replay_marks[LOG_MARK_CAP];static int replay_mark_count;
static u32 replay_time_ms;
static void replay_seek(u32 t){replay_time_ms=t;}
'''
main=r'''
int main(void){
 /* Test exact identifiers: never silently assume RPM and MAP from unrelated slots. */
 strcpy(log_channel_names[0],"RPMValue");strcpy(log_channel_names[1],"MAPValue");
 strcpy(tables[0].z,"fuelTable");strcpy(tables[0].xchan,"RPMValue");
 strcpy(tables[0].ychan,"MAPValue");table_count=1;cal[0].width=12;cal[0].height=16;
 int x=-1,y=-1;
 assert(trace_table_channels(0,&x,&y)==1&&x==0&&y==1);
 strcpy(tables[0].ychan,"unknownSensor");assert(trace_table_channels(0,&x,&y)==0);
 cal[0].height=1;assert(trace_table_channels(0,&x,&y)==1&&x==0&&y==-1);
 strcpy(tables[0].xchan,"");assert(trace_table_channels(0,&x,&y)==0);
 assert(trace_log_channel("MAPValue")==1);
 assert(trace_log_channel("something_else")==-1);
 /* Incremental coverage uses valid samples only and counts the correct 2D cell. */
 cal[0].width=3;cal[0].height=3;axis_ready[1]=true;
 for(int i=0;i<4;i++){
  replay_buffer[i].valid=3;
  replay_buffer[i].values[0]=(i<3)?100:200;
  replay_buffer[i].values[1]=100;
 }
 replay_buffer[2].valid=0;
 trace_coverage_tick(&cal[0]);
 assert(trace_coverage_done&&trace_valid_samples==3&&trace_coverage_at==4);
 assert(trace_coverage[4]==2&&trace_coverage[5]==1);

 /* Start with a Z + X-axis snapshot; verify two-stage undo and redo. */
 editor[0]=10;axis_data[0][0]=20;
 edit_history_capture(&edit_undo_before);
 editor[0]=11;edit_history_changed();
 edit_history_capture(&edit_undo_before);
 axis_data[0][0]=21;edit_history_changed();
 assert(edit_undo_count==3&&edit_undo_at==2);
 edit_history_move(-1);assert(editor[0]==11&&axis_data[0][0]==20);assert(undo_audio_count==1);
 edit_history_move(-1);assert(editor[0]==10&&axis_data[0][0]==20);assert(undo_audio_count==2);
 edit_history_move(1);assert(editor[0]==11&&axis_data[0][0]==20);assert(undo_audio_count==2);
 edit_history_capture(&edit_undo_before);
 editor[0]=13;edit_history_changed();
 assert(edit_undo_count==3&&edit_undo_at==2);
 edit_history_move(1);assert(editor[0]==13);
 edit_history_move(-1);assert(editor[0]==11);

 /* Real FS round-trip with app-generated filename only, marker removal, and seek. */
 assert(log_basename_valid("bricktuner-0001.csv"));
 assert(!log_basename_valid("../bricktuner-0001.csv"));
 assert(!log_basename_valid("bricktuner-00001.csv"));
 mkdir("sdmc:",0700);mkdir("sdmc:/BrickECU",0700);mkdir("sdmc:/BrickECU/logs",0700);
 for(int i=0;i<4;i++)replay_buffer[i].t_ms=(u32)(i*1000);
 replay_time_ms=1000;replay_mark_toggle();assert(replay_mark_count==1);
 replay_time_ms=2500;replay_mark_toggle();assert(replay_mark_count==2);
 replay_mark_count=0;replay_marks_load();assert(replay_mark_count==2);
 assert(replay_marks[0]==1000&&replay_marks[1]==2500);
 replay_time_ms=1000;replay_mark_jump(1);assert(replay_time_ms==2500);
 replay_mark_jump(-1);assert(replay_time_ms==1000);
 replay_mark_toggle();assert(replay_mark_count==1);
 replay_mark_count=0;replay_marks_load();assert(replay_mark_count==1&&replay_marks[0]==2500);
 puts("PASS actual C trace IDs, valid-sample map coverage, undo branching and persistent log marks");
 return 0;
}'''
with tempfile.TemporaryDirectory(prefix='bts-v31-') as d:
    d=Path(d)
    sourcefile=d/'test.c';exe=d/'test'
    sourcefile.write_text(C+'\n'+'\n'.join(grab(f) for f in funcs)+'\n'+main)
    cp=subprocess.run(['gcc','-std=gnu11','-O0','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-o',str(exe),str(sourcefile)],capture_output=True,text=True)
    if cp.returncode:raise RuntimeError(cp.stderr)
    run=subprocess.run([str(exe)],cwd=d,capture_output=True,text=True,timeout=15)
    print(run.stdout)
    if run.returncode:raise RuntimeError(run.stderr)
# Source-level contracts for page integration and honest labels.
for token in [
    'PAGE_TRACE_MAPS','replay_open_trace();return;','replay_open_selected_map(selected);',
    'replay_trace_active','LOG %lus  X %.0f  Y %.1f',
    'TAP ACTION  |  D-PAD SCRUB  SELECT MARK','map_touch_button(111,174,102,"UNDO",false)',
    'No matching map axes. Record the INI',
    'if(changed()&&!confirm_discard)',
    'B: DISCARD     A: KEEP EDITING',
]:assert token in source,token
print('PASS UI/action integration source contracts')
