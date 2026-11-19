from pathlib import Path
p=Path(__file__).resolve().parents[1]
a=(p/'web/app.js').read_text()
m=(p/'main/main.c').read_text()
t=(p/'main/telemetry.c').read_text()
assert "const rpm=authoritativeRpm();" not in a
assert "Fresh confirmed 0 RPM telemetry is required" not in a
assert "await freshStoppedRpm();" in a
assert "if(!idempotent || attempt>=2 || !transient)throw e;" in a
assert '\n#include "sdkconfig.h"\n' in m
assert "brick_live_holdoff(1800);" in m
assert "esp_timer_get_time()+60000000LL" in m
assert 'cJSON_AddStringToObject(j,"brick_os_build","v4.4");' in m
assert "s_holdoff_until_us" in t
print('PASS v4.4 cached telemetry gate removed, direct RPM check and USB write priority')
