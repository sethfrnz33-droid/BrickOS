#!/usr/bin/env python3
"""Check readOnly INI declarations and each read-only calibration guard."""
from pathlib import Path
import re,subprocess
root=Path(__file__).resolve().parents[1]
source=(root/'source/main.c').read_text()
fixture=root.parents[1]/'firmware/esp32/tests/fixtures/proteus_f7_generated.ini'
ini=fixture.read_text(errors='replace')
section='';names=[]
for line in ini.splitlines():
 line=line.strip()
 if line.startswith('[') and ']' in line:section=line[1:line.index(']')]
 if section=='ConstantsExtensions' and re.match(r'^readOnly\s*=',line):
    names.append(line.split('=',1)[1].split(';',1)[0].strip())
assert len(names)>=6,names
for field in ('device_uid1','device_uid2','device_uid3','gearCountArray','rangeStateArray'):
 assert field in names,field
for token in ('!strcasecmp(key,"readOnly")','if(f)f->readOnly=true;',
 'if(!f||f->readOnly||!f->validScale','if(cal[open_field].readOnly)',
 'if(f->readOnly){note("INI option is read-only");return;}',
 'return !f->readOnly && f->validScale',
 'if(f->readOnly){note("INI marks this map read-only");return;}'):
 assert token in source,token
subprocess.run(['gcc','-std=gnu11','-Wno-implicit-function-declaration',
 '-Wno-int-conversion','-Wno-incompatible-pointer-types','-fsyntax-only',
 '-I'+str(root/'source'),'-I'+str(root/'tests/host_stubs'),str(root/'source/main.c')],check=True)
print('PASS readOnly declarations',len(names),'and source guard coverage')
