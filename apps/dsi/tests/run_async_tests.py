#!/usr/bin/env python3
"""Local HTTP server splits chunk framing and streams >900KB through SD."""
import http.server
import json
import time
import pathlib
import socketserver
import struct
import subprocess
import threading
ROOT=pathlib.Path(__file__).resolve().parents[1]
PORT=18891
INI=("[TunerStudio]\n"
     'signature = "BrickECU altboard.2025.04.12.generic.1293678056"\n'
     '[Constants]\npage = 1\n'
     'rpmLimit = scalar, U16, 0, RPM, 1, 0\n'
     '[OutputChannels]\n'
     'RPMValue = scalar, U16, 0, RPM, 1, 0\n')
# Exercise the real page-definition CSV lines near the observed stall point.
# The previous >900 KB regression test mostly contained comment padding and
# never entered the parser functions with large automatic arrays.
PADDING="; padding that a real ECU INI can have without enormous memory usage\n"
INI+=PADDING*1400
INI+="[Constants]\npageIdentifier = "+", ".join(str(i) for i in range(1,65))+"\n"
INI+="pageSize = "+", ".join("2048" for _ in range(64))+"\n"
INI+="pageChunkWrite = "+", ".join("C%2i%2o%2c%v" for _ in range(64))+"\n"
INI+="burnCommand = "+", ".join("B%2i" for _ in range(64))+"\n"
INI+="[OutputChannels]\n"
INI+=PADDING*13600
class Handler(http.server.BaseHTTPRequestHandler):
    status_count=0
    pending_count=0
    sample_seq=122
    protocol_version='HTTP/1.1'
    def log_message(self,*args):pass
    def do_GET(self):
        if self.path=='/api/status':
            type(self).status_count+=1
            if type(self).status_count==2:
                time.sleep(1.0)
            b=json.dumps({'ecu_session_connected':True,'ecu_signature':'rusEFI master.2026.11.18.bigbrick.1293678056'}).encode()
        elif self.path=='/api/ini/file':
            self.send_response(200);self.send_header('Transfer-Encoding','chunked');self.end_headers()
            raw=INI.encode()
            for i in range(0,len(raw),307):
                chunk=raw[i:i+307]
                packet=f'{len(chunk):X}\r\n'.encode()+chunk+b'\r\n'
                self.wfile.write(packet[:2]);self.wfile.write(packet[2:])
            self.wfile.write(b'0\r\n\r\n');self.wfile.flush()
            return
        elif self.path.startswith('/api/live?'):
            if 'hz=50' not in self.path:
                self.send_error(400, 'DSi must use same live request as working 3DS')
                return
            if 'length=302' in self.path and type(self).pending_count<2:
                type(self).pending_count+=1
                b=b'{"error":"Waiting for first ECU telemetry sample"}'
                self.send_response(503, 'Service Unavailable')
                self.send_header('Content-Length',str(len(b)))
                self.end_headers()
                self.wfile.write(b);self.wfile.flush();return
            if 'length=300' in self.path:
                b=b'diagnostic: live length temporarily refused'
                self.send_response(488, 'BrickOS custom rejection')
                self.send_header('Content-Length',str(len(b)))
                self.end_headers()
                self.wfile.write(b);self.wfile.flush();return
            type(self).sample_seq+=1
            b=b'BLV1'+struct.pack('<I',type(self).sample_seq)+b'\0'*4+struct.pack('<H',256)+b'\0\0'+struct.pack('<H',1500)+b'\0'*254
        else:
            self.send_error(404);return
        self.send_response(200);self.send_header('Content-Length',str(len(b)));self.end_headers()
        self.wfile.write(b);self.wfile.flush()
class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address=True
    daemon_threads=True
with Server(('127.0.0.1',PORT),Handler) as s:
    t=threading.Thread(target=s.serve_forever,daemon=True);t.start()
    cmd=['gcc','-std=c11','-D_DEFAULT_SOURCE','-DARM9','-Wall','-Wextra','-Werror',
         '-DBTS_ESP_HOST="127.0.0.1"',f'-DBTS_ESP_PORT={PORT}',
         '-Isource','tests/test_async.c','source/wire.c','source/tuner.c','source/storage.c','-lm',
         '-o','/tmp/bts_async_test']
    subprocess.run(cmd,cwd=ROOT,check=True)
    subprocess.run(['/tmp/bts_async_test'],cwd=ROOT,check=True)
    s.shutdown()
