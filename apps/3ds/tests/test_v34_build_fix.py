#!/usr/bin/env python3
"""Regressions for the 3DS source manifest and safe bounded copy."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
makefile = (root / 'Makefile').read_text()
source = (root / 'source/main.c').read_text()
assert 'export OFILES := main.o font_assets.o' in makefile
assert '$(wildcard $(dir)/*.c)' not in makefile
assert '$(wildcard $(dir)/*.S)' not in makefile
assert 'garden_assets.o' not in makefile
assert 'pikmin_scene.o' not in makefile
assert 'instruments_assets.o' not in makefile
# Extract the exact bounded-copy implementation into an executable host test.
start = source.index('static void copy_text(')
end = source.index('\n}\n', start) + 2
func = source[start:end]
code = '#include <string.h>\n#include <stddef.h>\n#include <assert.h>\n' + func + '''
int main(void){
 char block[10];memset(block,'X',sizeof(block));
 copy_text(block+1,5,"abcdefghijk");
 assert(block[0]=='X' && !strcmp(block+1,"abcd") && block[6]=='X');
 copy_text(block+1,5,"");assert(block[1]==0 && block[6]=='X');
 copy_text(block+1,5,NULL);assert(block[1]==0 && block[6]=='X');
 copy_text(block+1,1,"long");assert(block[1]==0 && block[6]=='X');
 copy_text(block+1,0,"long");assert(block[1]==0 && block[6]=='X');
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='bts-v341-') as tmp:
    test=Path(tmp)/'copy_test.c';test.write_text(code)
    exe=Path(tmp)/'copy_test'
    subprocess.run(['gcc','-O2','-std=c11','-Wall','-Wextra','-Werror',str(test),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: fixed module manifest and bounded-copy edge cases')
