#!/usr/bin/env python3
"""Safe TPS/PPS capture source regressions; does not operate ECU hardware."""
from pathlib import Path
import re

root=Path(__file__).resolve().parents[3]
ini=(root/"firmware/esp32/tests/fixtures/proteus_f7_generated.ini").read_text(errors="replace")
bridge=(root/"firmware/esp32/main/main.c").read_text()
cache=(root/"firmware/esp32/main/ini_cache.c").read_text()
ds=(root/"apps/3ds/source/main.c").read_text()
expected={
    "cmd_calibrate_pedal_up":r'"Z\x00\x14\x00\x06"',
    "cmd_calibrate_pedal_down":r'"Z\x00\x14\x00\x07"',
    "cmd_calibrate_tps_closed":r'"Z\x00\x14\x00\x20"',
    "cmd_calibrate_tps_open":r'"Z\x00\x14\x00\x21"',
}
definitions={}
section=""
for raw in ini.splitlines():
    line=raw.strip()
    if line.startswith("[") and "]" in line:
        section=line[1:line.index("]")]
        continue
    if section!="ControllerCommands" or not line or line[0] in ";#" or "=" not in line:
        continue
    k,v=line.split("=",1)
    definitions[k.strip()]=v.strip()
for k,v in expected.items():
    assert definitions[k]==v,(k,definitions.get(k),v)
    assert re.search(r"commandButton\s*=.*\b"+k+r"\b",ini)
    assert '"'+k+'"' in bridge
    assert '"'+k+'"' in ds
    assert '"'+k+'"' in cache
assert 'capture_declaration_verified(command_id,value)' in cache
assert 'exact_signature(INI_PATH,ecu_signature)' in cache
assert 'info.capture_payload_verified' in bridge
assert 'static esp_err_t sensor_capture_post(' in bridge
assert '{.uri="/api/sensors/capture",.method=HTTP_POST,.handler=sensor_capture_post}' in bridge
assert "ts_request('Z',args,sizeof(args),reply,4500)" in bridge
assert 'stim_read_u16(&rpm_field,&rpm)' in bridge
assert 'fabs(rpm)>0.1' in bridge
assert 'stim_session_match(active,again,sizeof(again))' in bridge
assert 'stim_session_match(requested,again,sizeof(again))' not in bridge
assert 'capture_verified_by_readback",false' in bridge
assert 'TASK_SENSOR_CAPTURE' in ds
assert 'post_api("/api/sensors/capture",payload,11000)' in ds
assert 'if(sensor_capture_confirmation)' in ds
assert 'bts_demo_input(&demo_confirm' in ds
assert 'post_ts("Z"' not in ds
scheduler=ds[ds.index("static void run_scheduler(u64 now){"):]
assert scheduler.index('if(test_abort_pending){') < scheduler.index('if(sensor_capture_pending){')
assert 'if(!strchr("SVOARCBKe",op)' in bridge
print("PASS all four capture definitions, stop-state gate, native USB Z, 3DS two-step confirmation")
