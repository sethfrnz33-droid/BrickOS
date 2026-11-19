#!/usr/bin/env python3
"""Exercise v62-compatible protected bench writes against a local HTTP ESP mock."""
import http.server, json, pathlib, socketserver, subprocess, threading, struct
ROOT=pathlib.Path(__file__).resolve().parents[1]
PORT=18891
class Server(socketserver.ThreadingTCPServer): allow_reuse_address=True
class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    page=bytearray(256)
    page[32:36]=bytes((100,0,200,0))
    mode='success'
    writes=0;burns=0;backups=0;armed=False
    def log_message(self,*a):pass
    def sendb(self,doc):
        data=json.dumps(doc).encode();self.send_response(200)
        self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
    def do_GET(self):
        if self.path=='/api/status':
            self.sendb(dict(ecu_signature='test-ecu-v1',ecu_session_connected=True,
                            write_compiled=True,write_access_ready=True,backup_storage_ready=True))
        else:self.send_error(404)
    def do_POST(self):
        try:body=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        except Exception:self.send_error(400);return
        mode=type(self).mode
        if self.path=='/api/ts':
            cmd=body['cmd'];args=bytes.fromhex(body['args'])
            if cmd=='O':
                n=struct.unpack('<H',args[2:4])[0]
                self.sendb({'status':0,'hex':(b'\x50\x00' if mode=='running' else b'\0\0')[:n].hex()})
            elif cmd=='R':
                page,off,n=struct.unpack('<HHH',args)
                assert page==256
                self.sendb({'status':0,'hex':self.page[off:off+n].hex()})
            elif cmd=='C':
                assert self.armed and body['signature']=='test-ecu-v1'
                page,off,n=struct.unpack('<HHH',args[:6]);assert page==256 and n==len(args)-6
                self.page[off:off+n]=args[6:];type(self).writes+=1
                self.sendb({'status':0})
            elif cmd=='B':
                assert self.armed and body['signature']=='test-ecu-v1'
                type(self).burns+=1;self.sendb({'status':0})
            else:self.sendb({'status':1})
        elif self.path=='/api/backup':
            assert body['signature']=='test-ecu-v1' and body['page']==1
            type(self).backups+=1
            if mode=='backup_fail':self.sendb({'ok':False})
            else:self.sendb(dict(ok=True,stored='ESP32_STORAGE',page=1,length=256,crc32=123))
        elif self.path=='/api/arm':
            if mode=='arm_fail':self.sendb({'ok':False})
            else:
                assert body['confirm']=='BENCH_ONLY_ENGINE_STOPPED_BACKUP_SAVED'
                type(self).armed=True;self.sendb({'ok':True})
        elif self.path=='/api/disarm':
            type(self).armed=False;self.sendb({'ok':True})
        else:self.send_error(404)
subprocess.run(['gcc','-std=c11','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror',
 '-DBTS_ESP_HOST="127.0.0.1"',f'-DBTS_ESP_PORT={PORT}', '-Isource','tests/test_write.c',
 'source/tuner.c','source/wire.c','source/storage.c','-lm','-o','/tmp/bts_dsi_test_write'],cwd=ROOT,check=True)
with Server(('127.0.0.1',PORT),Handler) as server:
    threading.Thread(target=server.serve_forever,daemon=True).start()
    for mode in ('success','running','backup_fail','arm_fail','mismatch'):
        Handler.mode=mode;Handler.page[32:36]=bytes((100,0,200,0));Handler.armed=False
        a,b,c=Handler.writes,Handler.burns,Handler.backups
        subprocess.run(['/tmp/bts_dsi_test_write',mode],cwd=ROOT,check=True,timeout=30)
        if mode=='success':
            assert Handler.writes-a==1 and Handler.burns-b==1 and Handler.backups-c==1
        else:assert Handler.writes==a and Handler.burns==b
        assert not Handler.armed
        print('PASS: protected write gate',mode)
    server.shutdown()
