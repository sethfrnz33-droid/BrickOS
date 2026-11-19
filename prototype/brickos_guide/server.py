"""LOCALHOST ONLY test API. This API has NO ECU transport or calibration write route.

Intended as a contract prototype for future embedded implementation, NOT as a
production server exposed on the ESP AP.
"""
from __future__ import annotations
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse,parse_qs
from .core import GuidedEngine, SessionError
import json

class Handler(BaseHTTPRequestHandler):
    engine: GuidedEngine
    def _respond(self, status: int, data: dict):
        body=json.dumps(data,sort_keys=True).encode('utf8')
        self.send_response(status)
        self.send_header('Content-Type','application/json')
        self.send_header('Cache-Control','no-store')
        self.send_header('Content-Length',str(len(body)))
        self.end_headers()
        self.wfile.write(body)
    def _body(self):
        length=int(self.headers.get('Content-Length','0'))
        if not 0<length<=64*1024:
            raise ValueError('Body length must be between 1 and 65536 bytes')
        return json.loads(self.rfile.read(length).decode('utf8'))
    def do_GET(self):
        route=urlparse(self.path)
        if route.path=='/api/guided/v1/report':
            return self._respond(200,self.engine.report())
        if route.path=='/api/guided/v1/inventory':
            q=parse_qs(route.query)
            try:
                page=int(q.get('page',['0'])[0]); limit=int(q.get('limit',['100'])[0])
                return self._respond(200,self.engine.field_inventory(page,limit))
            except (ValueError,TypeError) as e:
                return self._respond(400,{'error':str(e)})
        if route.path=='/api/guided/v1/graph':
            return self._respond(200,{'edges':self.engine.graph.json()})
        if route.path=='/api/guided/v1/events':
            return self._respond(200,{'events':self.engine.audit})
        return self._respond(404,{'error':'not found'})
    def do_POST(self):
        try:
            data=self._body()
            route=urlparse(self.path).path
            if route=='/api/guided/v1/demo/definition':
                reply=self.engine.load_definition(data['ini'],data['ecu_signature'],data['revision'])
            elif route=='/api/guided/v1/demo/attach':
                reply=self.engine.attach()
            elif route=='/api/guided/v1/demo/heartbeat':
                reply=self.engine.heartbeat(data['token'])
            elif route=='/api/guided/v1/demo/detach':
                reply=self.engine.detach(data['token'])
            elif route=='/api/guided/v1/demo/sample':
                reply=self.engine.ingest(data['sample'],data['token'])
            elif route=='/api/guided/v1/demo/manual-revision':
                # Simulates manual editor change; does not edit calibration.
                reply={'stale_evidence':self.engine.calibration_changed(
                     set(data['changed_fields']),data['new_revision'])}
            else:
                return self._respond(404,{'error':'not found'})
            return self._respond(200,reply)
        except (SessionError,ValueError,KeyError,TypeError,json.JSONDecodeError) as e:
            return self._respond(400,{'error':type(e).__name__, 'message':str(e)})

def serve(host: str='127.0.0.1',port: int=8765):
    if host not in ('127.0.0.1','localhost','::1'):
        raise ValueError('Demo server must bind to loopback')
    h=type('BrickHandler',(Handler,),{'engine':GuidedEngine()})
    with ThreadingHTTPServer((host,port),h) as s:
        print(f'Offline BrickOS demo API listening on http://{host}:{port}')
        s.serve_forever()
