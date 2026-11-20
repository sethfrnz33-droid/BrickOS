#!/usr/bin/env python3
"""End-to-end HTTP regression for the INI reload/download path."""
import http.server, json, pathlib, subprocess, socketserver, tempfile, threading, shutil
ROOT=pathlib.Path(__file__).resolve().parents[1]
NEW_INI=('[TunerStudio]\nsignature="new.0987654321"\n'
         '[Constants]\npage=1\nfreshField=scalar,U16,2,"",1,0,0,100,0\n'
         '[OutputChannels]\nRPMValue=scalar,U16,0,"RPM",1,0\n').encode()
class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    fail_ini=False
    offline=False
    fetched=0
    def log_message(self,*args): pass
    def do_GET(self):
        if self.path=='/api/status':
            # No ecu_signature deliberately: v35 was stuck waiting for it.
            data=json.dumps({'ecu_session_connected':not type(self).offline}).encode();status=200
        elif self.path=='/api/ini/file':
            type(self).fetched+=1
            status=503 if type(self).fail_ini else 200
            data=b'Unavailable' if status==503 else NEW_INI
        else: status=404;data=b'Not found'
        self.send_response(status)
        self.send_header('Content-Length',str(len(data)))
        self.send_header('Connection','close')
        self.end_headers()
        try: self.wfile.write(data);self.wfile.flush()
        except (BrokenPipeError,ConnectionResetError): pass
class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address=True
    daemon_threads=True
with Server(('127.0.0.1',0),Handler) as server:
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    with tempfile.TemporaryDirectory(prefix='bts-v36-test-') as temp:
        exe=pathlib.Path(temp)/'refresh'
        port=server.server_address[1]
        cmd=['gcc','-std=gnu11','-D_DEFAULT_SOURCE','-O2','-Wall','-Wextra','-Werror',
             '-ffunction-sections','-fdata-sections','-Itests/stubs','-Isource',
             'tests/test_v36_ini_refresh.c','source/tuner.c','source/wire.c',
             'source/storage.c','source/dashboard.c','source/surface3d.c',
             '-lm','-Wl,--gc-sections','-o',str(exe),
             f'-DBTS_HOST_CACHE_DIR="{temp}/sd"','-DBTS_ESP_HOST="127.0.0.1"',
             f'-DBTS_ESP_PORT={port}']
        subprocess.run(cmd,check=True,cwd=ROOT)
        subprocess.run([exe,'success'],check=True,cwd=ROOT,timeout=25)
        assert Handler.fetched>0,'No /api/ini/file request made'
        Handler.fail_ini=True
        shutil.rmtree(pathlib.Path(temp)/'sd')
        subprocess.run([exe,'failure'],check=True,cwd=ROOT,timeout=25)
        Handler.fail_ini=False
        shutil.rmtree(pathlib.Path(temp)/'sd')
        subprocess.run([exe,'missing'],check=True,cwd=ROOT,timeout=25)
        Handler.offline=True
        shutil.rmtree(pathlib.Path(temp)/'sd')
        subprocess.run([exe,'offline'],check=True,cwd=ROOT,timeout=25)
    server.shutdown()
