#!/usr/bin/env python3
import http.server, socketserver, struct, json, pathlib, subprocess, threading
ROOT=pathlib.Path(__file__).resolve().parents[1]
PORT=18903
memory=bytearray(2048)
for off,numbers in ((4,[1000,2000,3000]),(12,[50,100]),(32,[100,110,120,130,140,150])):
    for idx,val in enumerate(numbers):struct.pack_into('<H',memory,off+idx*2,val)
struct.pack_into('<H',memory,16,123)
memory[20]=0xaa
for i in range(320):struct.pack_into('<H',memory,100+i*2,100+i)
class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    def log_message(self,*args):pass
    def do_POST(self):
        assert self.path=='/api/ts',self.path
        content=self.rfile.read(int(self.headers['Content-Length']))
        obj=json.loads(content)
        assert obj['cmd']=='R'
        page,offset,length=struct.unpack('<HHH',bytes.fromhex(obj['args']))
        assert page==256 and 0<=offset and offset+length<=2048,(page,offset,length)
        if offset==60:
            response=json.dumps({'ok':False,'error':'ECU busy'}).encode()
            self.send_response(503)
        else:
            response=json.dumps({'status':0,'length':length,'hex':memory[offset:offset+length].hex()}).encode()
            self.send_response(200)
        self.send_header('Content-Length',str(len(response)))
        self.end_headers();self.wfile.write(response);self.wfile.flush()
class Server(socketserver.ThreadingTCPServer):allow_reuse_address=True;daemon_threads=True
with Server(('127.0.0.1',PORT),Handler) as server:
    th=threading.Thread(target=server.serve_forever,daemon=True);th.start()
    cmd=['gcc','-std=gnu11','-D_DEFAULT_SOURCE','-O2','-Wall','-Wextra','-Werror',
         '-DBTS_ESP_HOST="127.0.0.1"',f'-DBTS_ESP_PORT={PORT}',
         '-Itests/stubs','-Isource','tests/test_v23_open.c','source/tuner.c',
         'source/wire.c','source/storage.c','source/dashboard.c','-lm','-o','/tmp/bts_v23_open']
    subprocess.run(cmd,cwd=ROOT,check=True)
    subprocess.run(['/tmp/bts_v23_open'],cwd=ROOT,check=True,timeout=30)
    server.shutdown()
