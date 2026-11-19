#!/usr/bin/env python3
"""Static protocol compatibility checks for Dream Tuner v1.9.
Does not simulate ECU flash or replace 3DS hardware tests.
Optional argument: path to the exact BrickOS main/main.c firmware source.
"""
from pathlib import Path
import re
import sys

src = (Path(__file__).parent / 'source/main.c').read_text()
checks = {
    'obsolete hardware interlock removed': 'physical_arm' not in src and 'hardware safety interlock not satisfied' not in src,
    'legacy v4.4 rejection removed': 'legacy 3DS burn API incompatible' not in src,
    'signed C and B': '"signature"' in src or 'signature\\\\\\"' in src,
    'signed payload sent': 'write?' in src and '"/api/ts"' in src and 'escape_json_string(ecu_signature' in src,
    'ESP32-side full-page backup': 'post_api("/api/backup",body,130000)' in src,
    'verified backup receipt checked': '"crc32"' in src and '"ESP32_STORAGE"' in src,
    'explicit arm operation': 'post_api("/api/arm",body,12500)' in src,
    'explicit disarm operation': 'post_api("/api/disarm","{}",12500)' in src,
    'stopped RPM direct ECU query': 'post_ts("O",hex)' in src and 'lookup("RPMValue",true)' in src,
    'baseline compared before any write': 'memcmp(check,p->baseline' in src and 'if(!arm_ecu())' in src,
    'write/readback verified': 'post_ts("C",hex)' in src and 'post_ts("B",hex)' in src,
    'verify after burn': 'Post-burn readback mismatch' in src,
    'no automatic write retry': 'int n=post_api("/api/ts",payload,12500)' in src,
    'INI commands gate per-page burn': 'parse_page_capability(val,page_burnable' in src and '!page_writeable[pg-1]||!page_burnable[pg-1]' in src,
    'only worker handles write': 'if(type==TASK_WRITE)' in src,
}
if len(sys.argv) > 1:
    server = Path(sys.argv[1]).read_text()
    checks.update({
        'server supports backup path': '{.uri="/api/backup"' in server,
        'server supports arm/disarm': '{.uri="/api/arm"' in server and '{.uri="/api/disarm"' in server,
        'server uses same arm token': 'BENCH_ONLY_ENGINE_STOPPED_BACKUP_SAVED' in server,
        'server accepts signed C and B': 'op==\'B\'||op==\'C\'' in server and 'requested_signature' in server,
        'server supplies RPM O queries': '"SVOARCBKe"' in server,
        'server returns backup CRC receipt': '"crc32"' in server and '"ESP32_STORAGE"' in server,
    })
for name, ok in checks.items():
    print(('PASS' if ok else 'FAIL') + ': ' + name)
assert all(checks.values()), 'Source protocol contract mismatch'
