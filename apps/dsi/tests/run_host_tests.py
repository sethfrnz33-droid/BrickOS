#!/usr/bin/env python3
"""Host-only test for the DSi wire contract; no ECU or DS needed."""
import http.server
import json
import pathlib
import socketserver
import struct
import subprocess
import threading
import os
ROOT = pathlib.Path(__file__).resolve().parents[1]
HOST='127.0.0.1'; PORT=18889
INI = ("[TunerStudio]\n"
       "signature = \"test-ecu-v1\"\n"
       "[Constants]\n"
       "pageIdentifier = \"\\x00\\x01\"\n"
       "page = 1\n"
       "veTable = array, U16, 32, [4x3], %, 0.1, 0\n"
       "[OutputChannels]\n"
       "RPMValue = scalar, U16, 0, RPM, 1, 0\n"
       "[TableEditor]\n"
       "table = veTable, fuelTable, \"Fuel VE\"\n"
       "zBins = veTable\n")
class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    def log_message(self,*args):pass
    def do_GET(self):
        if self.path=='/api/status':
            b=json.dumps({'ecu_session_connected':True,'ecu_signature':'test-ecu-v1'}).encode()
        elif self.path.startswith('/api/live?'):
            body=struct.pack('<H',1500)+b'\0'*254
            b=b'BLV1'+struct.pack('<I',123)+b'\0'*4+struct.pack('<H',256)+b'\0\0'+body
        elif self.path=='/api/ini/file':
            self.send_response(200)
            self.send_header('Transfer-Encoding','chunked')
            self.end_headers()
            raw=INI.encode()
            for i in range(0,len(raw),7):
                chunk=raw[i:i+7]
                framed=('%X\r\n'%len(chunk)).encode()+chunk+b'\r\n'
                for j in range(0,len(framed),3):self.wfile.write(framed[j:j+3])
            self.wfile.write(b'0\r\n\r\n')
            self.wfile.flush()
            return
        else:
            self.send_error(404);return
        self.send_response(200);self.send_header('Content-Length',str(len(b)));self.end_headers();self.wfile.write(b)
    def do_POST(self):
        data=self.rfile.read(int(self.headers.get('Content-Length',0)))
        op=json.loads(data)
        assert op['cmd']=='R'
        page,offset,length=struct.unpack('<HHH',bytes.fromhex(op['args']))
        assert page==256
        raw=bytes().join(struct.pack('<H',100+i*10) for i in range(12))
        b=json.dumps({'status':0,'hex':raw[offset-32:offset-32+length].hex()}).encode()
        self.send_response(200);self.send_header('Content-Length',str(len(b)));self.end_headers();self.wfile.write(b)
class Server(socketserver.ThreadingTCPServer): allow_reuse_address=True
with Server((HOST,PORT),Handler) as s:
    s.daemon_threads=True
    thread=threading.Thread(target=s.serve_forever,daemon=True);thread.start()
    cmd=['gcc','-std=c11','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror',
         '-DBTS_ESP_HOST="127.0.0.1"','-DBTS_ESP_PORT=18889',
         '-Isource','tests/test_wire.c','source/wire.c','source/tuner.c','source/storage.c','-lm',
         '-o','/tmp/bts_dsi_test_wire']
    subprocess.run(cmd,cwd=ROOT,check=True)
    subprocess.run(['/tmp/bts_dsi_test_wire'],cwd=ROOT,check=True)
    s.shutdown()
