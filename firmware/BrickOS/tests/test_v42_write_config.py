#!/usr/bin/env python3
"""v4.2 regression: legacy sdkconfig must not be able to lock out C/B support."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'main/main.c').read_text()
k=(root/'main/Kconfig.projbuild').read_text()
d=(root/'sdkconfig.defaults').read_text()
js=(root/'web/app.js').read_text()
assert 'CONFIG_BRICK_ALLOW_CAL_WRITES' not in s
assert 'BRICK_ALLOW_CAL_WRITES' not in k and 'BRICK_ALLOW_CAL_WRITES' not in d
assert 'cJSON_AddBoolToObject(j,"write_compiled",true);' in s
assert 'cJSON_AddBoolToObject(j,"write_access_ready",true);' in s
assert 'cJSON_AddStringToObject(j,"brick_os_build","v4.4");' in s
assert 'bool write=(op==\'B\'||op==\'C\')' in s
assert 'strchr("SVOARCBKe",op)' in s
assert 'if(write && esp_timer_get_time()>=armed_until)' in s
assert 'if(!runtime.write_compiled||!runtime.write_access_ready)' in js
assert "runtime=await api('/api/status')" in js
assert 'ESP32 is running ${build} with writes DISABLED' in js
assert "await api('/api/arm'" in js and "await api('/api/disarm'" in js
assert 'await freshStoppedRpm();' in js
assert 'Post-burn mismatch' in js
print('PASS v4.4: C/B compiled regardless of legacy sdkconfig; runtime verification and editor protections')
