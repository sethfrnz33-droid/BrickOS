"""Local test-only mock for browser integration. NEVER used in production firmware."""
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse, parse_qs
import json, pathlib, struct, time

ROOT=pathlib.Path(__file__).parents[1]/'web'
PAGES={0:bytearray(8192),256:bytearray(1024)}
PAGES[0][200:212]=struct.pack('<6H',150,260,360,220,300,510)
PAGES[0][450:452]=struct.pack('<h',-25)
ARMED=False
FILES={}
ACTIVE=False
AUTO=False

class Handler(BaseHTTPRequestHandler):
  def log_message(self,fmt,*args): pass
  def send(self,body,status=200,content_type='application/json'):
    if isinstance(body,(dict,list)):body=json.dumps(body).encode()
    if isinstance(body,str):body=body.encode()
    self.send_response(status);self.send_header('Content-Type',content_type);self.send_header('Content-Length',str(len(body)));self.end_headers();self.wfile.write(body)
  def do_GET(self):
    path=urlparse(self.path)
    if path.path=='/api/status':
      return self.send({'ok':True,'product':'BrickOS','transport':'mock CAN ISO-TP','can_tx_id':256,'can_rx_id':258,'tx_gpio':15,'rx_gpio':16,'last_ok_ms':2020,'ok_requests':23,'failed_requests':0,'last_error':'OK','write_compiled':True,'write_armed':ARMED,'physical_arm':True,'logging':ACTIVE,'samples':25,'log_name':'log_00000001.blg','storage':'/logs','storage_error':'OK','auto_log':AUTO,'auto_log_offset':0,'auto_log_length':256,'auto_log_interval_ms':100,'log_actual_hz':9.8,'log_failures':0,'log_last_sample_ms':10102})
    if path.path=='/api/logs':return self.send({'ok':True,'files':[{'name':x,'size':len(v)}for x,v in FILES.items()]})
    if path.path=='/api/log/get':
      name=parse_qs(path.query).get('name',[''])[0]
      return self.send(FILES.get(name,b''),content_type='application/octet-stream')
    if path.path in {'/','/app.css','/app.js','/core.mjs'}:
      filename=ROOT/('index.html' if path.path=='/' else path.path[1:])
      typ='text/html' if path.path=='/' else 'text/css' if path.path.endswith('css') else 'text/javascript'
      return self.send(filename.read_bytes(),content_type=typ)
    return self.send({'ok':False,'error':'Not found'},404)
  def do_POST(self):
    global ARMED,ACTIVE,FILES,AUTO
    size=int(self.headers.get('Content-Length',0));body=json.loads(self.rfile.read(size) or b'{}')
    if self.path=='/api/ts':
      c=body.get('cmd');b=bytes.fromhex(body.get('args',''))
      if c=='S':result=b'rusEFI sample-123\0';return self.send({'ok':True,'hex':result.hex(),'status':0})
      if c=='V':return self.send({'ok':True,'hex':b'rusEFI test\0'.hex(),'status':0})
      if c=='O':
        offset,count=struct.unpack('<HH',b);dat=bytearray(count)
        if not offset:
          dat[:4]=struct.pack('<HH',0,1000)
        return self.send({'ok':True,'hex':dat.hex(),'status':0})
      if c in ['R','C','B']:
        page=struct.unpack_from('<H',b,0)[0];data=PAGES[page]
        if c=='B':return self.send({'ok':True,'hex':'','status':4})
        off,count=struct.unpack_from('<HH',b,2)
        if c=='R':return self.send({'ok':True,'hex':data[off:off+count].hex(),'status':0})
        if not ARMED:return self.send({'ok':False,'error':'Not armed'},403)
        data[off:off+count]=b[6:6+count];return self.send({'ok':True,'hex':'','status':0})
      return self.send({'ok':False,'error':'Unknown cmd'},400)
    if self.path=='/api/arm':ARMED=True;return self.send({'ok':True,'armed_seconds':60})
    if self.path=='/api/disarm':ARMED=False;return self.send({'ok':True})
    if self.path=='/api/log/start':
      ACTIVE=True
      size=body['length'];b=bytearray(b'BRLOG1'+struct.pack('<HHH',0,size,body['interval_ms']))
      for i in range(30):
        r=bytearray(size)
        if size>=4:r[:4]=struct.pack('<HH',500+i*12,900+i)
        b.extend(struct.pack('<I',i*body['interval_ms'])+r)
      FILES['log_00000001.blg']=bytes(b);return self.send({'ok':True})
    if self.path=='/api/log/auto':AUTO=bool(body.get('enabled'));return self.send({'ok':True})
    if self.path=='/api/log/stop':ACTIVE=False;return self.send({'ok':True})
    return self.send({'ok':False,'error':'Unknown'},404)
  def do_DELETE(self):
    name=parse_qs(urlparse(self.path).query).get('name',[''])[0]
    FILES.pop(name,None);return self.send({'ok':True})

if __name__=='__main__':
  print('TEST MOCK ONLY — http://127.0.0.1:8065')
  ThreadingHTTPServer(('127.0.0.1',8065),Handler).serve_forever()
