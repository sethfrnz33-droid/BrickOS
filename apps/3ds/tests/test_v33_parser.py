#!/usr/bin/env python3
"""Compile and run exact saved-CSV parsing functions extracted from v3.3 source."""
from pathlib import Path
import tempfile,subprocess
src=(Path(__file__).resolve().parents[1]/'source/main.c').read_text()
start=src.index('static bool log_parse_header_named(')
end=src.index('static void calculate_replay_statistics(',start)
real=src[start:end]
code='''#include <assert.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <ctype.h>
#define LOG_FIELDS 9
typedef uint32_t u32;
typedef uint16_t u16;
typedef struct {u32 t_ms;float values[LOG_FIELDS];u16 valid;} LogSample;
static char log_channel_names[LOG_FIELDS][64];
'''+real+'''
int main(void){
 char names[9][64]={{0}};
 char header[]="elapsed_ms,AFR,RPM,MAP,TPS,Coolant,Battery,Ignition,Oil,Knock\\n";
 assert(log_parse_header_named(header,names));
 assert(!strcmp(names[0],"AFR")&&!strcmp(names[8],"Knock"));
 char row[]="123,14.3,,101.2,8,79,13.5,,60,1\\n";
 LogSample r;assert(log_parse_record(row,&r));
 assert(r.t_ms==123&&(r.valid&1)&&!(r.valid&2)&&(r.valid&4));
 assert(fabs(r.values[0]-14.3)<1e-4);
 char bad[]="123,14,invalid,101.2,8,79,13.5,,60,1\\n";
 assert(!log_parse_record(bad,&r));
 char shortrow[]="123,14,1\\n";
 assert(!log_parse_record(shortrow,&r));
 puts("PASS real csv parser and 9-channel optional missing-value validation");
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder)/'test.c'; p.write_text(code)
 exe=Path(folder)/'parser'
 subprocess.run(['gcc','-O2','-Wall','-Wextra','-std=c11',str(p),'-lm','-o',str(exe)],check=True)
 print(subprocess.run([str(exe)],capture_output=True,text=True,check=True).stdout.strip())
