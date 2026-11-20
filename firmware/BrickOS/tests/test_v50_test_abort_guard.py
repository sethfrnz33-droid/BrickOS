#!/usr/bin/env python3
"""Source-level guard for the actual ECU test-abort path (never test-start).

The static regression checks the exact generated rusEFI command, strict
signature validation, a fixed payload, no arbitrary test command forwarding,
and unambiguous status reporting. It does not claim hardware validation.
"""
from pathlib import Path
import re

root=Path(__file__).resolve().parents[3]
bridge=(root/"firmware/esp32/main/main.c").read_text()
ds=(root/"apps/3ds/source/main.c").read_text()
ini=(root/"firmware/esp32/tests/fixtures/proteus_f7_generated.ini").read_text()
assert 'cmd_test_cancel = "Z\\x00\\x16\\x00\\x0f"' in ini
assert 'static esp_err_t test_abort_post(' in bridge
func=bridge.split("static esp_err_t test_abort_post(",1)[1].split("static esp_err_t stim_stop_post(",1)[0]
assert 'strcmp(requested,active)==0' in func
assert 'vcp_gateway_usb_ready()' in func
assert 'const uint8_t cancel_args[]={0x00,0x16,0x00,0x0f};' in func
assert "ts_request('Z',cancel_args,sizeof(cancel_args),reply,4500)" in func
assert 'command_acknowledged' in func and 'running_state_verified' in func
assert 'error_reply(r,503,' in func
assert '{.uri="/api/test/abort",.method=HTTP_POST,.handler=test_abort_post}' in bridge
assert 'if(!strchr("SVOARCBKe",op)' in bridge
assert 'SVOARCBKeZ' not in bridge
assert 'TASK_TEST_ABORT' in ds
assert '"/api/test/abort"' in ds
assert 'cmd_test_cancel' in ds
assert 'if(test_abort_pending)' in ds
assert 'ECU test abort ACKNOWLEDGED' in ds
assert 'request_output_test_abort();' in ds
assert 'post_ts("Z"' not in ds
print("PASS fixed abort, live-signature check, ack reporting; no unrestricted test-start path")
