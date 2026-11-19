#!/usr/bin/env python3
"""Host-native behavior tests for confirmed-session fault reset and safe CSV removal."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()

def extract(signature):
    start=s.index(signature)
    opening=s.index('{',start)
    depth=0
    for i in range(opening,len(s)):
        if s[i]=='{':depth+=1
        elif s[i]=='}':
            depth-=1
            if depth==0:return s[start:i+1]
    raise ValueError(signature)

fault=r'''
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
typedef uint32_t u32;
static int last_http=200;
static char payload[512];
static bool ecu_session=true;
static bool fault_session_was_lost=false, fault_poll_requested=false,
            fault_recheck_after_reconnect=false,fault_active=true;
static bool write_compiled,write_armed,esp_write_access_ready,esp_backup_ready,
            stim_stop_supported,stim_start_supported,esp_status_valid,
            stim_on_by_bridge;
static char stim_server_state[48],esp_firmware_build[32],ecu_signature[160]="TEST",
            fault_ecu_signature[160]="TEST",fault_message[384]="Old ECU fault",
            fault_candidate[384]="Old ECU fault";
static u32 esp_wifi_clients,esp_disconnect_reason;
static int fault_link_count=2,fault_clear_count=1,fault_candidate_count=2;
static unsigned long long fault_last_enqueue=4321,fault_last_success=222;
static int ui_page=1,fault_return_page=9,selected=42,scroll_y=42;
enum {PAGE_FIELD=1,PAGE_FAULT=3};
static int field_return_page=PAGE_FAULT;
static int notes=0;
static void note(const char *s){assert(s);notes++;}
static void nav_restore(void){}
static char *http_body(void){return payload;}
static bool json_number(const char *p,const char *key,double *out){(void)p;(void)key;(void)out;return false;}
static bool json_bool(const char *p,const char *key,bool *out){
 char needle[90];snprintf(needle,sizeof(needle),"\"%s\":",key);
 const char *at=strstr(p,needle);if(!at)return false;
 at+=strlen(needle);
 if(!strncmp(at,"true",4)){*out=true;return true;}
 if(!strncmp(at,"false",5)){*out=false;return true;}
 return false;
}
static bool json_string(const char *p,const char *key,char *dest,int cap){
 (void)p;(void)key;(void)dest;(void)cap;return false;
}
'''+extract('static void decode_status(void){')+r'''
int main(void){
 strcpy(payload,"{\"ecu_session_connected\":false}");
 decode_status();
 assert(!ecu_session&&fault_session_was_lost&&fault_active);
 strcpy(payload,"{\"ecu_session_connected\":true}");
 decode_status();
 assert(ecu_session&&!fault_active&&!fault_link_count);
 assert(!fault_candidate_count&&!fault_clear_count);
 assert(fault_poll_requested&&fault_recheck_after_reconnect&&fault_last_enqueue==0);
 assert(!fault_session_was_lost&&notes==1);
 assert(field_return_page==fault_return_page);
 fault_active=true;strcpy(fault_message,"New fault");
 strcpy(payload,"{\"ignored\":true}");
 decode_status();
 assert(fault_active); /* malformed status is not a reconnect */
 puts("PASS: disconnect keeps old fault; confirmed reconnect invalidates it and re-polls");
 return 0;
}
'''

log=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#define LOG_FILE_CAP 96
#define BTS_SOUND_UNDO 12
static char log_filenames[LOG_FILE_CAP][48],log_delete_name[48]="";
static int log_file_count=0,log_browser_selected=0,log_browser_scroll=0,selected=0;
static bool log_delete_pending=false,log_recording=false,event_audio_enabled=true;
static char replay_filename[48]="bricktuner-0001.csv",compare_filename[48]="bricktuner-0001.csv";
static int replay_count=5,replay_mark_count=3,compare_count=4;
static bool replay_playing=true,replay_trace_active=true;
static void *compare_buffer=NULL;
static int sound_count=0,last_sound=-1;
static char last_note[200]="";
static void note(const char *s){snprintf(last_note,sizeof(last_note),"%s",s);}
static bool bts_audio_play(int cue,bool enabled){if(enabled){sound_count++;last_sound=cue;}return enabled;}
'''+extract('static void copy_text(char *dest, size_t cap, const char *src){')+"\n"+extract('static bool log_basename_valid(const char *name){')+"\n"+extract('static void refresh_log_files(void){')+"\n"+extract('static void log_delete_request(int index){')+"\n"+extract('static void log_delete_confirm(void){')+r'''
int main(void){
 assert(mkdir("sdmc:",0777)==0);
 assert(mkdir("sdmc:/BrickECU",0777)==0);
 assert(mkdir("sdmc:/BrickECU/logs",0777)==0);
 const char *csv="sdmc:/BrickECU/logs/bricktuner-0001.csv";
 char side[160];
 FILE *f=fopen(csv,"wb");assert(f);fputs("header\n",f);fclose(f);
 snprintf(side,sizeof(side),"%s.marks",csv);
 f=fopen(side,"wb");assert(f);fputs("marks",f);fclose(f);
 snprintf(side,sizeof(side),"%s.session.txt",csv);
 f=fopen(side,"wb");assert(f);fputs("metadata",f);fclose(f);
 compare_buffer=malloc(32);assert(compare_buffer);
 refresh_log_files();assert(log_file_count==1);
 log_delete_request(0);
 assert(log_delete_pending&&sound_count==0);
 log_delete_confirm();
 assert(!log_delete_pending&&sound_count==1&&last_sound==BTS_SOUND_UNDO);
 assert(log_file_count==0&&replay_count==0&&compare_count==0&&!compare_buffer);
 assert(!replay_filename[0]&&!compare_filename[0]);
 assert(fopen(csv,"rb")==NULL);
 snprintf(side,sizeof(side),"%s.marks",csv);assert(fopen(side,"rb")==NULL);
 snprintf(side,sizeof(side),"%s.session.txt",csv);assert(fopen(side,"rb")==NULL);
 log_delete_request(0);assert(!log_delete_pending&&sound_count==1);
 strcpy(log_delete_name,"../outside.csv");log_delete_pending=true;
 log_delete_confirm();assert(sound_count==1);
 puts("PASS: Y/A confirmed log deletion, path validation, sidecars, cache invalidation, KidPix once");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    td=Path(td)
    for name,program in [('session',fault),('delete',log)]:
        src=td/f'{name}.c';src.write_text(program)
        exe=td/name
        result=subprocess.run(['gcc','-std=gnu11','-O2','-Wall','-Wextra',
               '-Wno-unused-variable','-Wno-unused-function',str(src),'-o',str(exe)],capture_output=True,text=True)
        assert result.returncode==0,result.stderr
        result=subprocess.run([str(exe)],cwd=td,capture_output=True,text=True)
        assert result.returncode==0,(result.stdout,result.stderr)
        print(result.stdout.strip())

# Actual UI wiring (not a decorative button)
assert 'if(k&KEY_Y){log_delete_request(selected);return;}' in s
assert 'if(log_delete_pending)' in s
assert 'if(ui_page==PAGE_LOG_BROWSER&&tp.py>=206){log_delete_request(selected);return;}' in s
assert 'bts_context_named(label,target);' in s
assert 'if(bts_context_last_at[cue] && now-bts_context_last_at[cue]<2500ULL)' in s
print('PASS: real UI and context trigger wiring')
