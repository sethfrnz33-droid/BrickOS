from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'main/ini_cache.c').read_text()
fixture=root/'tests/fixtures/proteus_f7_generated.ini'
sig='rusEFI master.2026.11.09.proteus_f7.2735508644'
with tempfile.TemporaryDirectory(prefix='brick-stim-ini-') as temp:
    path=Path(temp); (path/'ecu.ini').write_bytes(fixture.read_bytes())
    source=source.replace('#define INI_PATH "/logs/ecu.ini"',f'#define INI_PATH "{path}/ecu.ini"')
    program=path/'ini.c'
    program.write_text(source+f'''
#include <assert.h>
#include <math.h>
int xSemaphoreTake(SemaphoreHandle_t s, uint32_t t){{(void)s;(void)t;return 1;}}
int xSemaphoreGive(SemaphoreHandle_t s){{(void)s;return 1;}}
int main(void) {{
    lock=(SemaphoreHandle_t)1;initialized=true;
    brick_bench_u16_t r={{0}},v={{0}};
    assert(ini_cache_bench_channels("{sig}",&r,&v));
    assert(r.offset==4 && r.scale==1 && r.translate==0);
    assert(v.offset==36 && fabs(v.scale-0.001)<1e-10 && v.translate==0);
    assert(!ini_cache_bench_channels("wrong ECU",&r,&v));
    puts("PASS: actual cached INI parses RPM/voltage only for matching ECU signature");
    return 0;
}}
''')
    exe=path/'run'
    subprocess.run(['gcc','-std=gnu17','-O2','-Wno-implicit-function-declaration',
     '-Wno-int-conversion','-ffunction-sections','-fdata-sections',
     '-I'+str(root/'tests/host_stubs'),'-I'+str(root/'main'),'-include',str(root/'tests/host_stubs/main_extra.h'),
     str(program),'-Wl,--gc-sections','-lm','-o',str(exe)],check=True,capture_output=True,text=True)
    result=subprocess.run([str(exe)],capture_output=True,text=True,check=True)
    print(result.stdout.strip())
