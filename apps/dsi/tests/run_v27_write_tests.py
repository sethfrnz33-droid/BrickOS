#!/usr/bin/env python3
"""DSi v27: actual cooperative HTTP write transactions, no real ECU needed."""
from pathlib import Path
import http.server, json, socketserver, subprocess, threading, struct, time
ROOT=Path(__file__).resolve().parents[1]
PORT=18892
class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address=True
    daemon_threads=True
class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    page=bytearray(256)
    mode='success'
    writes=burns=backups=arms=disarms=0
    armed=False
    def log_message(self,*a):pass
    def sendb(self,doc):
        data=json.dumps(doc).encode();self.send_response(200)
        self.send_header('Content-Length',str(len(data)))
        self.end_headers();self.wfile.write(data)
    def do_GET(self):
        if self.path=='/api/status':
            if self.mode=='status_stall':
                time.sleep(3);return
            self.sendb(dict(ecu_signature='test-ecu-v1',ecu_session_connected=True,
                write_compiled=True,write_access_ready=True,backup_storage_ready=True))
        else:self.send_error(404)
    def do_POST(self):
        try:body=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        except Exception:self.send_error(400);return
        cls=type(self);mode=cls.mode
        if self.path=='/api/ts':
            cmd=body['cmd'];args=bytes.fromhex(body['args'])
            if cmd=='O':
                n=struct.unpack('<H',args[2:4])[0]
                self.sendb({'status':0,'hex':(b'\x50\x00' if mode=='running' else b'\0\0')[:n].hex()})
            elif cmd=='R':
                page,off,n=struct.unpack('<HHH',args)
                assert page==256
                b=self.page[off:off+n]
                if mode=='mismatch' and off==32 and cls.writes==0:b=b'\0'*n
                self.sendb({'status':0,'hex':b.hex()})
            elif cmd=='C':
                assert cls.armed and body['signature']=='test-ecu-v1'
                page,off,n=struct.unpack('<HHH',args[:6]);assert page==256
                cls.page[off:off+n]=args[6:];cls.writes+=1
                self.sendb({'status':0})
            elif cmd=='B':
                assert cls.armed and body['signature']=='test-ecu-v1'
                cls.burns+=1
                if mode=='burn_stall':
                    time.sleep(3);return
                self.sendb({'status':0})
            else:self.sendb({'status':1})
        elif self.path=='/api/backup':
            cls.backups+=1
            if mode=='backup_fail':self.sendb({'ok':False})
            else:self.sendb(dict(ok=True,stored='ESP32_STORAGE',page=1,length=256,crc32=123))
        elif self.path=='/api/arm':
            cls.arms+=1
            if mode=='arm_fail':self.sendb({'ok':False})
            else:cls.armed=True;self.sendb({'ok':True})
        elif self.path=='/api/disarm':
            cls.disarms+=1;cls.armed=False;self.sendb({'ok':True})
        else:self.send_error(404)
exe='/tmp/bts-dsi-v27-write'
subprocess.run(['gcc','-std=c11','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror','-O2',
    '-DBTS_ESP_HOST="127.0.0.1"',f'-DBTS_ESP_PORT={PORT}', '-Isource',
    'tests/test_v27_write.c','source/tuner.c','source/wire.c','source/storage.c',
    '-lm','-o',exe],cwd=ROOT,check=True)
with Server(('127.0.0.1',PORT),Handler) as server:
    threading.Thread(target=server.serve_forever,daemon=True).start()
    for mode in ('success','cancel_prearm','cancel_after_arm','wifi_lost_prearm','wifi_lost_after_arm','running','backup_fail','arm_fail','mismatch','status_stall','burn_stall'):
        Handler.mode=mode
        Handler.page[32:36]=bytes((100,0,200,0));Handler.armed=False
        Handler.writes=Handler.burns=Handler.backups=Handler.arms=Handler.disarms=0
        run=subprocess.run([exe,mode],cwd=ROOT,capture_output=True,text=True,timeout=20)
        print(mode,run.returncode,run.stdout.strip(),run.stderr[-240:].strip())
        assert run.returncode==0,(mode,run.stdout,run.stderr)
        if mode in ('success','cancel_after_arm'):
            assert (Handler.writes,Handler.burns,Handler.backups,Handler.arms,Handler.disarms)==(1,1,1,1,1)
        if mode in ('running','backup_fail','arm_fail','mismatch','status_stall','cancel_prearm','wifi_lost_prearm','wifi_lost_after_arm'):
            assert Handler.writes==0 and Handler.burns==0
        if mode=='burn_stall':
            assert Handler.writes==1 and Handler.burns==1 and Handler.disarms==1
        assert not Handler.armed
    server.shutdown()
print('PASS v27 all cooperative protected burn scenarios')
