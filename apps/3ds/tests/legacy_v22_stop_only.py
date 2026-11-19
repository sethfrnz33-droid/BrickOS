import pathlib,subprocess,tempfile,http.server,threading,json
root=pathlib.Path(__file__).resolve().parents[1]
source=(root/'source/main.c').read_text().replace('"192.168.4.1"','"127.0.0.1"').replace('#define HOST_PORT 80','#define HOST_PORT 18081')
fw_path=pathlib.Path(__file__).resolve().parents[2]/'BrickOS_v46/main/main.c'
if fw_path.exists():
 fw=fw_path.read_text()
 assert "ts_request('Z',args,sizeof(args),reply,4500)" in fw
 assert 'strchr("SVOARCBKe",op)' in fw
 assert '"stim_remote_start_supported",false' in fw
else:
 print('SKIP: separate BrickOS v4.6 firmware source not bundled')
with tempfile.TemporaryDirectory() as td:
 p=pathlib.Path(td)
 (p/'mock.c').write_text('#define main original_main\n'+source+'''\n#undef main
int hidKeysHeld(void){return 0;}
int main(int argc,char**argv){
 response=(char*)malloc(RESPONSE_CAP);
 net_ok=true;wifi_candidate=true;ecu_session=true;stim_stop_supported=true;
 Resolved r;memset(&r,0,sizeof(r));r.type=3;
 snprintf(r.command,sizeof(r.command),"%s",argc>1&&argv[1][0]=='E'?"cmd_enable_self_stim":"cmd_disable_self_stim");
 preview_stim_action(&r);
 fprintf(stderr,"note=%s\\n",ui_note);
 free(response);
 return 0;
}
''')
 exe=p/'mock';subprocess.run(['gcc','-std=gnu11','-O0','-ffunction-sections','-fdata-sections','-Wno-implicit-function-declaration','-Wno-int-conversion','-Wno-incompatible-pointer-types','-Wno-pointer-to-int-cast',f'-I{root}/tests/host_stubs',f'-I{root}/source',str(p/'mock.c'),'-Wl,--gc-sections','-lm','-o',str(exe)],check=True,capture_output=True)
 requests=[]
 class Handler(http.server.BaseHTTPRequestHandler):
  def log_message(self,*args):pass
  def do_POST(self):
   requests.append((self.path,self.rfile.read(int(self.headers.get('Content-Length','0')))))
   val={'ok':True,'command_acknowledged':True,'running_state_verified':False}
   body=json.dumps(val,separators=(',',':')).encode();self.send_response(200);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
 http=http.server.HTTPServer(('127.0.0.1',18081),Handler);threading.Thread(target=http.serve_forever,daemon=True).start()
 try:
  x=subprocess.run([str(exe),'E'],capture_output=True,text=True,timeout=5)
  assert x.returncode==0 and 'Hazardous-test warning' in x.stderr and not requests,(x.stderr,requests)
  print('PASS: start never sends ECU request')
  x=subprocess.run([str(exe),'D'],capture_output=True,text=True,timeout=5)
  assert x.returncode==0 and 'stop acknowledged' in x.stderr.lower() and len(requests)==1 and requests[0][0]=='/api/stim/stop',(x.stderr,requests)
  print('PASS: disable sends real stop request and reports only acknowledgement')
 finally:http.shutdown()
print('PASS: stop-only hardware interface contract')
