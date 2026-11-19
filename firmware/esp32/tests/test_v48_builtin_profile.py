"""Compiles and executes the exact v4.8 builtin-profile function in isolation.
It must work when no INI cache exists, but only for the exact known rusEFI build.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'main/main.c').read_text()
start = source.index('static const char *stim_builtin_signature =')
end = source.index('static bool stim_session_match(', start)
part = source[start:end]
with tempfile.TemporaryDirectory(prefix='brick-v48-') as temp:
    src = Path(temp) / 'profile.c'
    src.write_text('''
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef struct { unsigned offset; double scale, translate; } brick_bench_u16_t;
static bool ini_cache_bench_channels(const char *s,brick_bench_u16_t *r,brick_bench_u16_t *b)
{ (void)s; (void)r; (void)b; return false; }
''' + part + '''
int main(void){
  brick_bench_u16_t r={0}, b={0};
  assert(stim_get_channels("rusEFI master.2026.11.09.proteus_f7.2735508644",&r,&b));
  assert(r.offset==4 && r.scale==1.0 && r.translate==0.0);
  assert(b.offset==36 && b.scale==0.001 && b.translate==0.0);
  assert(!stim_get_channels("rusEFI master.2026.11.10.proteus_f7.changed",&r,&b));
  puts("PASS: no cached INI needed for exact firmware; other firmware rejected");
  return 0;
}
''')
    exe = Path(temp) / 'profile'
    subprocess.run(['cc','-std=gnu17','-O2','-Wall','-Wextra','-Werror',str(src),'-o',str(exe)], check=True)
    subprocess.run([str(exe)],check=True)
