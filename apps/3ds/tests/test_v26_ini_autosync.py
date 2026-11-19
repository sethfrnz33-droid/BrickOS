#!/usr/bin/env python3
"""Native 3DS HTTP simulation: missing Bridge INI gets synced from 3DS SD."""
from pathlib import Path
import subprocess, tempfile, json, http.server, threading
root=Path(__file__).resolve().parents[1]
source=(root/'source/main.c').read_text().replace('"192.168.4.1"','"127.0.0.1"').replace('#define HOST_PORT 80','#define HOST_PORT 18085')
signature='rusEFI master.2026.11.09.proteus_f7.2735508644'
with tempfile.TemporaryDirectory(prefix='brick-auto-ini-') as temp:
    t=Path(temp)
    sd=t/'sdmc:'/'BrickECU'; sd.mkdir(parents=True)
    # A stand-alone generated matching INI; no external development-tree file.
    definition=(f'[TunerStudio]\nsignature = "{signature}"\n[Constants]\n'
       'page = 1\n[OutputChannels]\n'
       'RPMValue = scalar, U16, 4, "RPM", 1, 0\n'
       'VBatt = scalar, U16, 36, "V", 0.001, 0\n'
       + '; test filler\n'*510).encode()
    (sd/'ecu.ini').write_bytes(definition)
    native=t/'main.c'
    native.write_text('#define main original_app_main\n'+source+f'''
#undef main
int main(int argc,char **argv){{
    response=malloc(RESPONSE_CAP);
    if(!response)return 5;
    net_ok=true;wifi_candidate=true;ecu_session=true;menu_loaded=true;
    strcpy(ecu_signature,"{signature}");strcpy(ini_signature,ecu_signature);
    int ok=stim_http_action(TASK_STIM_PREPARE);
    if(argc>1&&argv[1][0]=='R'){{
      if(ok>=0)return 15; /* non-INI failure cannot initiate upload */
      free(response);return 0;
    }}
    if(ok!=1||strcmp(stim_token,"2ac47100")||stim_last_rpm!=0)return 10;
    free(response);return 0;
}}
''')
    exe=t/'run'
    subprocess.run(['gcc','-std=gnu11','-O2','-ffunction-sections','-fdata-sections',
       '-Wno-implicit-function-declaration','-Wno-int-conversion','-Wno-incompatible-pointer-types',
       '-Wno-pointer-to-int-cast','-I'+str(root/'tests/host_stubs'),'-I'+str(root/'source'),
       str(native),'-Wl,--gc-sections','-lm','-o',str(exe)],check=True,capture_output=True,text=True)
    seen=[]; phase={'name':'missing'}
    class Api(http.server.BaseHTTPRequestHandler):
        def log_message(self,*a):pass
        def reply(self,body,status=200):
            raw=json.dumps(body,separators=(',',':')).encode()
            self.send_response(status); self.send_header('Content-Type','application/json');
            self.send_header('Content-Length',str(len(raw))); self.end_headers(); self.wfile.write(raw)
        def do_POST(self):
            content=self.rfile.read(int(self.headers['Content-Length']))
            seen.append(self.path)
            if self.path=='/api/stim/preflight':
                assert json.loads(content)=={'signature':signature}
                if phase['name']=='rpm':return self.reply({'error':'ECU reports nonzero RPM'},403)
                if seen.count('/api/stim/preflight')==1:
                    if phase['name']=='legacy':
                        return self.reply({'error':'Preflight blocked: require verified INI, fresh RPM 0 (unless stim already active), and measured VBatt below 1 V'},403)
                    return self.reply({'error':'No matching INI or built-in firmware channel profile'},403)
                return self.reply({'ok':True,'rpm':0,'vbat':0,'token':'2ac47100'})
            if self.path=='/api/ini/upload':
                assert len(content)>5000
                assert signature.encode() in content
                assert b'RPMValue = scalar, U16, 4' in content
                assert b'VBatt = scalar, U16, 36' in content
                return self.reply({'ok':True})
            raise AssertionError(self.path)
    server=http.server.HTTPServer(('127.0.0.1',18085),Api)
    threading.Thread(target=server.serve_forever,daemon=True).start()
    try:
        p=subprocess.run([str(exe)],cwd=str(t),capture_output=True,text=True,timeout=30)
        assert p.returncode==0,(p.returncode,p.stderr,p.stdout,seen)
        assert seen==['/api/stim/preflight','/api/ini/upload','/api/stim/preflight'],seen
        print('PASS: missing ESP32 INI auto-syncs 3DS SD cache then retries independent preflight')
        phase['name']='legacy';seen.clear()
        p=subprocess.run([str(exe)],cwd=str(t),capture_output=True,text=True,timeout=30)
        assert p.returncode==0,(p.returncode,p.stderr,p.stdout,seen)
        assert seen==['/api/stim/preflight','/api/ini/upload','/api/stim/preflight'],seen
        print('PASS: v4.7 legacy generic INI error also attempts one matching SD transfer')
        phase['name']='rpm';seen.clear()
        p=subprocess.run([str(exe),'R'],cwd=str(t),capture_output=True,text=True,timeout=20)
        assert p.returncode==0,(p.returncode,p.stderr,p.stdout,seen)
        assert seen==['/api/stim/preflight'],seen
        print('PASS: non-INI preflight rejection never uploads an INI or attempts Start')
        # A mismatch in the DS UI copy cannot upload an unverified document.
        # Original fixture still never reaches the Start endpoint without preflight.
    finally:server.shutdown()
