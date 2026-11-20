#!/usr/bin/env python3
"""Read-only TunerStudio action coverage and interface contract regression.

This checks the exact generated rusEFI INI and static wiring. No physical
output is activated and no ECU communication is performed.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
INI = (ROOT / "firmware/esp32/tests/fixtures/proteus_f7_generated.ini").read_text(errors="replace")
CACHE = (ROOT / "firmware/esp32/main/ini_cache.c").read_text()
BRIDGE = (ROOT / "firmware/esp32/main/main.c").read_text()
THREEDS = (ROOT / "apps/3ds/source/main.c").read_text()

defined = {}
buttons = []
section = None
for raw in INI.splitlines():
    line = raw.strip()
    if line.startswith("[") and "]" in line:
        section = line[1:line.index("]")]
        continue
    if not line or line[0] in ";#" or "=" not in line:
        continue
    key, value = (part.strip() for part in line.split("=", 1))
    if section == "ControllerCommands":
        if key in defined:
            raise AssertionError(f"Duplicate command: {key}")
        defined[key] = value
    if section == "UserDefined" and key == "commandButton":
        match = re.match(r'"([^"]+)"\s*,\s*([A-Za-z_][A-Za-z0-9_]*)', value)
        assert match, f"Unparsed commandButton: {value}"
        buttons.append((match.group(1), match.group(2)))

missing = [(title, command) for title, command in buttons if command not in defined]
assert not missing, missing[:10]
assert len(buttons) == 124, len(buttons)
assert len({command for _, command in buttons}) == 116
assert len(defined) == 124

assert 'bool ini_cache_command_info(' in CACHE
catalog = CACHE[CACHE.index('bool ini_cache_command_info('):]
assert 'exact_signature(INI_PATH,ecu_signature)' in catalog
assert 'xSemaphoreGive(lock)' in catalog
assert 'SECTION_COMMANDS' in catalog and 'SECTION_USER' in catalog
assert 'ts_request(' not in catalog
assert 'httpd_resp_send(' not in catalog
assert 'command_id' in catalog

assert 'static esp_err_t ts_inspect_get(' in BRIDGE
assert '{.uri="/api/ts/inspect",.method=HTTP_GET,.handler=ts_inspect_get}' in BRIDGE
assert 'cJSON_AddBoolToObject(j,"ecu_command_sent",false)' in BRIDGE
assert 'cJSON_AddBoolToObject(j,"hardware_actuation_supported",false)' in BRIDGE
assert 'if(!strchr("SVOARCBKe",op)' in BRIDGE
assert 'strchr("SVOARCBKeZ",op)' not in BRIDGE

assert 'TASK_COMMAND_INSPECT' in THREEDS
assert '"/api/ts/inspect?name=%s"' in THREEDS
assert 'if(is_stim_action(&results[selected]))' in THREEDS
assert 'else if(type==TASK_COMMAND_INSPECT)' in THREEDS
assert 'if(command_inspect_pending)' in THREEDS
assert 'post_ts("Z"' not in THREEDS
assert 'post_api("/api/ts/inspect"' not in THREEDS
assert 'command_inspect_name' in THREEDS
print(f"PASS {len(buttons)} INI buttons / {len(set(x[1] for x in buttons))} unique command refs")
print("PASS read-only, exact-signature Bridge inspection and guarded 3DS UI wiring")
