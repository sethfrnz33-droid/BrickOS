#!/usr/bin/env python3
"""Native regression against the real rusEFI Proteus F7 INI; no ECU access."""
from pathlib import Path
import ctypes, math, subprocess, tempfile, sys

ROOT=Path(__file__).resolve().parents[1]
FIXTURE=Path(sys.argv[1]) if len(sys.argv)>1 else ROOT.parents[1]/'firmware/esp32/tests/fixtures/proteus_f7_generated.ini'
assert FIXTURE.is_file(), f"Missing Proteus F7 fixture: {FIXTURE}"
class Flags(ctypes.Structure):
    _fields_=[('values',ctypes.c_double*6),('known',ctypes.c_bool*6)]

def csv(line):
    out=[];word='';quoted=False;depth=0
    for c in line:
        if c=='"':quoted=not quoted
        if not quoted:
            if c in '{[(':depth+=1
            elif c in '}])':depth-=1
            if c==';' and depth<=0:break
            if c==',' and depth<=0:out.append(word.strip());word='';continue
        word+=c
    out.append(word.strip())
    return out

section='';fields=[]
for line in FIXTURE.read_text(errors='replace').splitlines():
    row=line.strip()
    if row.startswith('[') and row.endswith(']'):section=row[1:-1];continue
    if section not in ('Constants','OutputChannels') or not row or row[0] in '#;' or '=' not in row:continue
    key,value=row.split('=',1);items=csv(value)
    if not items or items[0] not in ('scalar','array'):continue
    pos=4 if items[0]=='array' else 3
    if len(items)>pos+4 and '{' in items[pos+1]:fields.append((key.strip(),items,pos))

with tempfile.TemporaryDirectory() as td:
    source=Path(td)/'unit.c';lib=Path(td)/'unit.so'
    source.write_text('#include "bts_ini_numeric.h"\n'
        'int test_eval(const char *s,const BtsIniFlags *f,double *out){return bts_ini_numeric(s,f,out);}\n')
    subprocess.run(['gcc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-function',
        '-shared','-fPIC',str(source),'-I'+str(ROOT/'source'),'-lm','-o',str(lib)],check=True)
    fn=ctypes.CDLL(str(lib)).test_eval
    fn.argtypes=[ctypes.c_char_p,ctypes.POINTER(Flags),ctypes.POINTER(ctypes.c_double)]
    fn.restype=ctypes.c_int
    def evaluate(value,flags):
        result=ctypes.c_double()
        return result.value if fn(value.encode(),ctypes.byref(flags),ctypes.byref(result)) else None
    unknown=Flags()
    assert evaluate('{useMetricOnInterface ? 1 : 0.145038}',unknown) is None
    assert evaluate('{unknownVar ? 1 : 2}',unknown) is None
    assert evaluate('{1/0}',unknown) is None
    assert evaluate('{1/10}',unknown)==0.1
    assert len(fields)>=100,len(fields)
    runs=0
    for metric in (0,1):
      for lam in (0,1):
       for fuel in (0,1):
        for mode in (0,1,2):
         flags=Flags()
         for i,val in enumerate((metric,lam,fuel,mode,mode,mode)):
            flags.values[i]=val;flags.known[i]=True
         for key,parts,pos in fields:
            scale=evaluate(parts[pos+1],flags)
            trans=evaluate(parts[pos+2],flags)
            low=evaluate(parts[pos+3],flags)
            high=evaluate(parts[pos+4],flags)
            assert None not in (scale,trans,low,high),(key,parts)
            assert math.isfinite(scale) and math.isfinite(trans) and scale!=0 and low<=high,key
            runs+=1
    assert runs==24*len(fields)
source=(ROOT/'source/main.c').read_text()
for symbol in ('TASK_INI_FLAGS','ini_resolve_all_numeric_fields()','ini_drop_dynamic_conversions()',
               'read_cal_constant(bts_ini_flag_names[i],&value)'):
    assert symbol in source,symbol
assert 'task_axis_scale_known' not in source
subprocess.run(['gcc','-std=gnu11','-Wno-implicit-function-declaration',
 '-Wno-int-conversion','-Wno-incompatible-pointer-types','-fsyntax-only',
 '-I'+str(ROOT/'source'),'-I'+str(ROOT/'tests/host_stubs'),str(ROOT/'source/main.c')],check=True)
print(f'PASS: {len(fields)} actual INI scales x 24 contexts = {runs} checked conversions')
