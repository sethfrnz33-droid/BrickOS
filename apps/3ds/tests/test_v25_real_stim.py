from pathlib import Path
import subprocess, tempfile, http.server, threading, json
root=Path(__file__).resolve().parents[1]
src=(root/'source/main.c').read_text().replace('"192.168.4.1"','"127.0.0.1"').replace('#define HOST_PORT 80','#define HOST_PORT 18082')
with tempfile.TemporaryDirectory(prefix='brick-stim-ds-') as temp:
    tmp=Path(temp)
    test=tmp/'mock.c'
    test.write_text('#define main unused_ds_main\n'+src+'''
#undef main
int main(int argc,char **argv){
    response=(char*)malloc(RESPONSE_CAP);
    if(!response)return 10;
    net_ok=true;wifi_candidate=true;ecu_session=true;menu_loaded=true;
    strcpy(ini_signature,"bench-ecu");strcpy(ecu_signature,"bench-ecu");
    int result=stim_http_action(TASK_STIM_PREPARE);
    if(argc>1 && argv[1][0]=='X'){free(response);return result<0?0:11;}
    if(result!=1||strcmp(stim_token,"abcd1234")||stim_last_rpm!=0||stim_last_vbat>1){fprintf(stderr,"r=%d tok=%s rpm=%f bat=%f note=%s\\n",result,stim_token,stim_last_rpm,stim_last_vbat,ui_note);return 12;}
    result=stim_http_action(TASK_STIM_START);
    if(result!=1||stim_token[0])return 13;
    result=stim_http_action(TASK_STIM_STOP);
    free(response);
    return result==1?0:14;
}
''')
    exe=tmp/'test'
    subprocess.run(['gcc','-std=gnu11','-O2','-ffunction-sections','-fdata-sections',
        '-Wno-implicit-function-declaration','-Wno-int-conversion','-Wno-incompatible-pointer-types',
        '-Wno-pointer-to-int-cast','-I'+str(root/'tests/host_stubs'),'-I'+str(root/'source'),
        str(test),'-Wl,--gc-sections','-lm','-o',str(exe)],check=True,capture_output=True)
    seen=[]; mode='normal'
    class Api(http.server.BaseHTTPRequestHandler):
        def log_message(self,*args):pass
        def reply(self,obj,code=200):
            data=json.dumps(obj,separators=(',',':')).encode();self.send_response(code)
            self.send_header('Content-Type','application/json')
            self.send_header('Content-Length',str(len(data)))
            self.end_headers();self.wfile.write(data)
        def do_POST(self):
            data=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
            seen.append((self.path,data))
            if self.path=='/api/stim/preflight':
                assert data=={'signature':'bench-ecu'}
                if mode=='rejected':return self.reply({'ok':False,'error':'ECU RPM nonzero'},403)
                return self.reply({'ok':True,'token':'abcd1234','already_active':False,'rpm':0,'vbat':0})
            if self.path=='/api/stim/start':
                assert data=={'signature':'bench-ecu','token':'abcd1234','confirm':'BENCH_NO_12V_LR_CONFIRMED'}
                return self.reply({'ok':True,'command_acknowledged':True})
            if self.path=='/api/stim/stop':
                assert data=={}
                return self.reply({'ok':True,'command_acknowledged':True})
            raise AssertionError(self.path)
    server=http.server.HTTPServer(('127.0.0.1',18082),Api)
    threading.Thread(target=server.serve_forever,daemon=True).start()
    try:
        r=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
        assert r.returncode==0,(r.returncode,r.stdout,r.stderr,seen)
        assert [p for p,b in seen]==['/api/stim/preflight','/api/stim/start','/api/stim/stop'],seen
        print('PASS: DS uses real preflight -> signed Start -> Stop commands in order')
        mode='rejected';seen.clear()
        r=subprocess.run([str(exe),'X'],capture_output=True,text=True,timeout=10)
        assert r.returncode==0,(r.returncode,r.stdout,r.stderr,seen)
        assert [p for p,b in seen]==['/api/stim/preflight'],seen
        print('PASS: rejected preflight never sends ECU Start')
    finally:server.shutdown()
