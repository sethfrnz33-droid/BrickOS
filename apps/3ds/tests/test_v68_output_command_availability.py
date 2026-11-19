#!/usr/bin/env python3
"""Guard against advertising or invoking unsupported physical-output tests.

The supplied rusEFI INI has ControllerCommands ('Z' actions). The current
BrickOS /api/ts endpoint rejects Z; UI code must not pretend to execute them.
This is a metadata/host-source regression, NOT a hardware-actuation test.
"""
from pathlib import Path
import re

root=Path(__file__).resolve().parents[1]
source=(root/"source/main.c").read_text()
esp=(root.parents[1]/"firmware/esp32/main/main.c").read_text()
ini=(root.parents[1]/"firmware/esp32/tests/fixtures/proteus_f7_generated.ini").read_text(errors="replace")

assert "[ControllerCommands]" in ini
assert 'cmd_test_main_relay' in ini and 'cmd_test_fuel_pump' in ini
assert 'cmd_test_starter_relay' in ini and 'cmd_test_cancel' in ini
assert re.search(r'cmd_test_main_relay\s*=\s*"Z\\x00\\x16\\x00\\x00"',ini)
assert len(re.findall(r"(?m)^\s*commandButton\s*=",ini))>=100
assert 'if(!strchr("SVOARCBKe",op)' in esp
assert 'strchr("SVOARCBKeZ",op)' not in esp

# When the user opens an INI command button, no fake injector/ignition
# acknowledgement may be offered; only the existing guarded trigger path runs.
start=source.index('else if(ui_page==PAGE_DETAIL){if(selected>=0&&selected<result_count){')
end=source.index('else if(ui_page==PAGE_SEARCH)',start)
action=source[start:end]
assert 'if(is_stim_action(&results[selected]))' in action
assert 'else if(is_sensor_capture_command(results[selected].command))' in action
assert 'else note("Physical output test unavailable in BrickOS")' in action
assert 'warn_before_hazardous_test(hazard)' not in action

assert '"CAPTURE"' in source
assert '"NO BRIDGE"' in source
assert '"A REAL SENSOR CAPTURE / L+R CONFIRM"' in source
assert 'static void draw_config_dialog_bottom(void)' in source
assert 'config_group_count()' in source
assert 'TASK_COMMAND_INSPECT' in source
assert '/api/ts/inspect?name=%s' in source
print('PASS real TPS/pedal calibration, abort, guarded trigger and disabled unsupported commands')
print('PASS no unsupported actuator dispatch or deceptive confirmation screen')
