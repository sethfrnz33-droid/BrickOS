"""python -m brickos_guide [demo | server | inspect INI_FILE]"""
import json
import sys
from pathlib import Path
from .ini import parse_ini
from .sim import demo
from .server import serve

def main():
    mode=sys.argv[1] if len(sys.argv)>1 else 'demo'
    if mode=='demo':
        engine=demo()
        print(json.dumps(engine.report(),indent=2))
        return
    if mode=='server':
        serve()
        return
    if mode=='inspect' and len(sys.argv)==3:
        inv=parse_ini(Path(sys.argv[2]).read_text(encoding='utf8',errors='replace'))
        print(json.dumps(inv.summary(),indent=2))
        print(json.dumps({'unknown_examples':inv.unknown[:5],
                          'unprocessed_directives':inv.directives[:5]},indent=2))
        return
    raise SystemExit('Usage: python -m brickos_guide [demo | server | inspect FILE.ini]')

if __name__=='__main__':
    main()
