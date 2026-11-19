import pathlib, subprocess, threading, json, http.server, binascii, tempfile
root=pathlib.Path(__file__).resolve().parent.parent
tmp=tempfile.TemporaryDirectory(prefix="dream-tuner-write-test-")
build=pathlib.Path(tmp.name)
source=(root/'source/main.c').read_text().replace('"192.168.4.1"','"127.0.0.1"').replace('#define HOST_PORT 80','#define HOST_PORT 18080')
testfile=build/'native_mock_v19.c'
testfile.write_text('static unsigned long long mock_millis(void){return 1000ULL;}\n#define osGetTime mock_millis\n#define main ds_unused_main\n#define LightLock_Lock(lock) ((void)0)\n#define LightLock_Unlock(lock) ((void)0)\n'+source+'''
#undef main
int main(void){
 response=(char*)malloc(RESPONSE_CAP);
 if(!response)return 1;
 net_ok=true;wifi_candidate=true;
 snprintf(ini_signature,sizeof(ini_signature),"fake-ecu-signature");
 page_ids[0]=0;page_sizes[0]=16;page_ids_count=1;page_burnable[0]=page_writeable[0]=true;
 Field *v=&channels[0];memset(v,0,sizeof(*v));
 snprintf(v->name,sizeof(v->name),"RPMValue");v->kind=K_SCALAR;v->type=T_U16;
 v->validScale=true;v->offset=4;v->scale=1;chan_count=1;
 Field *f=&cal[0];memset(f,0,sizeof(*f));
 snprintf(f->name,sizeof(f->name),"testField");f->kind=K_SCALAR;f->type=T_U16;
 f->validScale=true;f->page=1;f->offset=1;f->bytes=2;f->width=1;f->height=1;
 cal_count=1;open_field=0;editor_loaded=staged=true;editor_length=2;
 original[0]=0x20;original[1]=0x00;
 editor[0]=0x2a;editor[1]=0x00;
 perform_queued_write();
 fprintf(stderr,"RESULT verified=%d staged=%d note=%s\\n",(int)write_verified,(int)staged,ui_note);
 free(response);
 return write_verified&&!staged?0:10;
}
''')
subprocess.run(['gcc','-std=gnu11','-O2','-Wno-stringop-overflow','-ffunction-sections','-fdata-sections','-Wno-implicit-function-declaration','-Wno-int-conversion','-Wno-incompatible-pointer-types','-Wno-pointer-to-int-cast','-I'+str(root/'tests/host_stubs'),'-I'+str(root/'source'),str(testfile),'-Wl,--gc-sections','-lm','-o',str(build/'native_mock_v19')], check=True)
logs=[];scenario="success"
data=bytearray(16);data[1]=0x20
class Api(http.server.BaseHTTPRequestHandler):
 def log_message(self,*args):pass
 def reply(self,obj,status=200):
  bs=json.dumps(obj,separators=(',',':')).encode()
  self.send_response(status);self.send_header('Content-Type','application/json');self.send_header('Content-Length',str(len(bs)));self.end_headers();self.wfile.write(bs)
 def do_GET(self):
  if self.path=='/api/status':
   return self.reply({'ok':True,'ecu_session_connected':True,'write_compiled':True,'write_access_ready':True,'backup_storage_ready':True,'ecu_signature':'fake-ecu-signature','brick_os_build':'v4.5','physical_arm':False,'write_armed':False})
  self.reply({'ok':False},404)
 def do_POST(self):
  body=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
  logs.append((self.path,body))
  if self.path=='/api/backup':
   if scenario=='backup-fails': return self.reply({'ok':False,'error':'Storage unavailable'},503)
   assert (body['page'],body['page_id'],body['size'],body['signature'])==(1,0,16,'fake-ecu-signature')
   return self.reply({'ok':True,'page':1,'length':16,'crc32':binascii.crc32(data),'stored':'ESP32_STORAGE'})
  if self.path=='/api/arm':
   assert body['confirm']=='BENCH_ONLY_ENGINE_STOPPED_BACKUP_SAVED'
   assert body['signature']=='fake-ecu-signature'
   return self.reply({'ok':True,'armed_seconds':60})
  if self.path=='/api/disarm':return self.reply({'ok':True})
  if self.path=='/api/ts':
   cmd=body['cmd'];args=bytes.fromhex(body['args']);status=0;replydata=b''
   if cmd=='O':
    assert args==bytes([4,0,2,0]);replydata=bytes([100,0]) if scenario=='rpm-running' else bytes([0,0])
   elif cmd=='R':
    page,off,amount=int.from_bytes(args[:2],'little'),int.from_bytes(args[2:4],'little'),int.from_bytes(args[4:6],'little')
    assert page==0;replydata=data[off:off+amount]
   elif cmd=='C':
    if scenario=='write-rejected': return self.reply({'ok':False,'error':'ECU rejected calibration'},503)
    assert body['signature']=='fake-ecu-signature'
    off=int.from_bytes(args[2:4],'little');amount=int.from_bytes(args[4:6],'little')
    assert amount==len(args[6:]);data[off:off+amount]=args[6:]
   elif cmd=='B':
    if scenario=='burn-rejected': return self.reply({'ok':False,'error':'Flash busy'},503)
    assert body['signature']=='fake-ecu-signature'
    status=4
   else:raise AssertionError(cmd)
   return self.reply({'ok':True,'status':status,'hex':bytes(replydata).hex(),'length':len(replydata)})
  self.reply({'ok':False,'error':'unknown'},404)
server=http.server.HTTPServer(('127.0.0.1',18080),Api)
th=threading.Thread(target=server.serve_forever,daemon=True);th.start()
try:
 for scenario in ['success','backup-fails','rpm-running','baseline-conflict','write-rejected','burn-rejected']:
  logs.clear();data[:]=bytes(16);data[1]=0x21 if scenario=='baseline-conflict' else 0x20
  p=subprocess.run([str(build/'native_mock_v19')],capture_output=True,text=True,timeout=30)
  steps=[k+':'+v.get('cmd','') for k,v in logs]
  print(scenario, p.stderr.strip(), 'ops:',steps)
  if scenario=='success':
   assert p.returncode==0 and 'verified=1' in p.stderr
   assert [v.get('cmd') or k for k,v in logs]==['O','/api/backup','R','O','/api/arm','O','C','R','B','R','/api/disarm']
   assert data[1]==0x2a
  elif scenario=='backup-fails':
   assert p.returncode==10 and 'verified=0' in p.stderr and not any(k=='/api/arm' or v.get('cmd') in ['C','B'] for k,v in logs)
   assert data[1]==0x20
  elif scenario=='rpm-running':
   assert p.returncode==10 and 'verified=0' in p.stderr and len(logs)==1
   assert data[1]==0x20
  elif scenario=='baseline-conflict':
   assert p.returncode==10 and 'verified=0' in p.stderr
   assert data[1]==0x21 and not any(k=='/api/arm' or v.get('cmd') in ['C','B'] for k,v in logs)
  elif scenario=='burn-rejected':
   assert p.returncode==10 and 'verified=0' in p.stderr
   assert steps[-1]=='/api/disarm:' and sum(v.get('cmd')=='B' for k,v in logs)==1
  elif scenario=='write-rejected':
   assert p.returncode==10 and 'verified=0' in p.stderr
   assert steps[-1]=='/api/disarm:' and not any(v.get('cmd')=='B' for k,v in logs)
   assert data[1]==0x20
 print('PASS all 6 native mock protocol scenarios')
finally:server.shutdown()
