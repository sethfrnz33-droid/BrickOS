from pathlib import Path
import re, tempfile, subprocess
p=Path(__file__).resolve().parents[1]
source=(p/'main/main.c').read_text()
needle='static uint32_t crc_step('
start=source.index(needle)
brace=source.index('{',start)
depth=1;at=brace+1
while depth:
    if source[at]=='{':depth+=1
    elif source[at]=='}':depth-=1
    at+=1
impl=source[start:at]
with tempfile.TemporaryDirectory() as tmp:
    f=Path(tmp)/'crc.c';out=Path(tmp)/'crc'
    f.write_text('#include <stdint.h>\n#include <stddef.h>\n'+impl+'\n#include <stdio.h>\nint main(void){const uint8_t v[]="123456789";uint32_t c=crc_step(0xffffffffU,v,9)^0xffffffffU;if(c!=0xcbf43926U){printf("%08x",c);return 1;}puts("CRC32 PASS");return 0;}\n')
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(f),'-o',str(out)],check=True)
    subprocess.run([str(out)],check=True)
print('PASS: native ESP32 backup CRC implementation matches reference')
