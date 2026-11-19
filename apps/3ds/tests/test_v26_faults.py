#!/usr/bin/env python3
"""Native tests for REAL Brick Tuner v2.6 rusEFI error plumbing and UI links."""
from pathlib import Path
import subprocess, tempfile, http.server, threading, json
root=Path(__file__).resolve().parents[1]
# UI contract: top display overlay is applied after every live/map/dashboard view.
ui_source=(root/'source/main.c').read_text()
assert ui_source.index('else home_dashboard();\n       draw_fault_top_overlay();')>0
assert 'draw_fault_top_overlay();' in ui_source
assert 'if(fault_active && (k&KEY_Y)' in ui_source
src=(root/'source/main.c').read_text().replace('"192.168.4.1"','"127.0.0.1"').replace('#define HOST_PORT 80','#define HOST_PORT 18084')
with tempfile.TemporaryDirectory(prefix='brick-faults-') as directory:
    test=Path(directory)/'faults.c'
    test.write_text('#define main unused_ds_main\n'+src+'''
#undef main
/* Provide platform stubs only for code pulled in by navigation. */
int osGetTime(void){return 1000;}
void DSP_FlushDataCache(void *b, size_t n){(void)b;(void)n;}
void ndspChnSetFormat(int c,int f){(void)c;(void)f;}
void ndspChnSetRate(int c,float r){(void)c;(void)r;}
void ndspChnSetInterp(int c,int t){(void)c;(void)t;}
void ndspChnSetMix(int c,float m[12]){(void)c;(void)m;}
void ndspChnWaveBufClear(int n){(void)n;}
void ndspChnWaveBufAdd(int n,void *b){(void)n;(void)b;}
void swkbdInit(void*a,int b,int c,int d){(void)a;(void)b;(void)c;(void)d;}
void swkbdSetHintText(void*a,const char*b){(void)a;(void)b;}
void swkbdSetInitialText(void*a,const char*b){(void)a;(void)b;}
int swkbdInputText(void*a,char*b,unsigned int c){(void)a;(void)b;(void)c;return 0;}
int hidKeysHeld(void){return 0;}
static void init_3ds(void){
 response=malloc(RESPONSE_CAP);
 if(!response)abort();
 net_ok=true;wifi_candidate=true;ecu_session=true;menu_loaded=true;
 snprintf(ini_signature,sizeof(ini_signature),"proteus-test");
 snprintf(ecu_signature,sizeof(ecu_signature),"proteus-test");
 cal_count=3;
 snprintf(cal[0].name,sizeof(cal[0].name),"triggerInputPins1");
 snprintf(cal[1].name,sizeof(cal[1].name),"flexSensorPin");
 snprintf(cal[2].name,sizeof(cal[2].name),"unusedInputPin");
 for(int i=0;i<cal_count;i++){
   cal[i].width=1;cal[i].height=1;cal[i].bytes=2;cal[i].type=T_U16;
   cal[i].page=1;cal[i].offset=10+2*i;cal[i].validScale=true;cal[i].scale=1;
 }
}
int main(void){
 init_3ds();
 /* Live rusEFI command: not a fabricated local fault. */
 if(post_ts("e","")<0)return 10;
 unsigned char text[BRICK_FAULT_MAX];int n=get_hex_reply(text,BRICK_FAULT_MAX-1);
 if(n<1)return 11;text[n]=0;
 fault_accept_message((const char*)text);
 if(fault_active)return 30; /* one isolated error reply is not enough */
 fault_accept_message((const char*)text);
 if(!fault_active||fault_link_count!=2)return 12;
 if(strcmp(cal[fault_links[0].field].name,"triggerInputPins1")||
    strcmp(cal[fault_links[1].field].name,"flexSensorPin"))return 13;
 ui_page=PAGE_CATS;selected=2;scroll_y=1;
 fault_open_page();
 if(ui_page!=PAGE_FAULT)return 14;
 selected=0;enter_selected();
 if(ui_page!=PAGE_FIELD||open_field!=0||field_return_page!=PAGE_FAULT)return 15;
 back();if(ui_page!=PAGE_FAULT)return 16;
 selected=1;enter_selected();
 if(ui_page!=PAGE_FIELD||open_field!=1)return 17;
 back();if(ui_page!=PAGE_FAULT)return 18;
 back();if(ui_page!=PAGE_CATS||selected!=2||scroll_y!=1)return 19;
 /* An unrecognized message must NEVER manufacture an unrelated shortcut. */
 fault_accept_message("Unrecognized ECU internal issue");
 if(fault_link_count!=2||!fault_active)return 20; /* do not replace on one reply */
 fault_accept_message("Unrecognized ECU internal issue");
 if(fault_link_count!=0||!fault_active)return 31;
 fault_accept_message("Pin PA3 required by trigger input but is used by flex fuel");
 if(fault_link_count!=0)return 32; /* new message still unconfirmed */
 fault_accept_message("Pin PA3 required by trigger input but is used by flex fuel");
 if(fault_link_count!=2)return 21;
 fault_accept_message("");if(!fault_active)return 22;
 /* Polling failures are ignored, so they cannot clear a genuine alert. */
 if(fault_link_count!=2)return 23;
 fault_accept_message("");if(fault_active||fault_link_count)return 24;
 free(response);
 puts("PASS rusEFI e transport; two matching reports; verified pin links; clear confirmation");
 return 0;
}
''')
    exe=Path(directory)/'faults'
    subprocess.run(['gcc','-std=gnu11','-O2','-ffunction-sections','-fdata-sections',
      '-Wno-implicit-function-declaration','-Wno-int-conversion','-Wno-incompatible-pointer-types',
      '-Wno-pointer-to-int-cast','-I'+str(root/'tests/host_stubs'),'-I'+str(root/'source'),
      str(test),'-Wl,--gc-sections','-lm','-o',str(exe)],check=True)
    requests=[]
    class Api(http.server.BaseHTTPRequestHandler):
       def log_message(self,*args):pass
       def do_POST(self):
          data=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
          requests.append((self.path,data))
          assert self.path=='/api/ts' and data['cmd']=='e' and data['args']==''
          txt=b'Pin PA3 required by trigger input but is used by flex fuel'
          body=json.dumps({'ok':True,'status':0,'length':len(txt),'hex':txt.hex()},separators=(',',':')).encode()
          self.send_response(200);self.send_header('Content-Type','application/json')
          self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
    server=http.server.HTTPServer(('127.0.0.1',18084),Api)
    threading.Thread(target=server.serve_forever,daemon=True).start()
    try:
       p=subprocess.run([str(exe)],capture_output=True,text=True,timeout=12)
       assert p.returncode==0,(p.returncode,p.stdout,p.stderr)
       assert len(requests)==1,requests
       print(p.stdout.strip())
    finally:server.shutdown()
