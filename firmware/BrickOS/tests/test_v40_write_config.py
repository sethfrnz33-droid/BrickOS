#!/usr/bin/env python3
"""Regression test for the real firmware write path, not the browser mock."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'main/main.c').read_text()
c=(r/'main/Kconfig.projbuild').read_text()
d=(r/'sdkconfig.defaults').read_text()
a=(r/'web/app.js').read_text()
assert 'strchr("SVOARCBKe",op)' in s, 'B flash burn must be accepted by HTTP dispatch'
assert "bool write=(op=='B'||op=='C')" in s
assert "ts_request(op,bytes,n,reply,timeout)" in s
assert 'CONFIG_BRICK_ALLOW_CAL_WRITES' not in s
assert 'BRICK_ALLOW_CAL_WRITES' not in c and 'BRICK_ALLOW_CAL_WRITES' not in d
assert 'BRICK_WRITE_ARM_GPIO' not in s and 'BRICK_WRITE_ARM_GPIO' not in c
assert 'esp_timer_get_time()>=armed_until' in s
assert 'strcmp(active_signature,requested_signature)!=0' in s
assert 'signature_ok' in s
assert "ts('C',writeArgs" in a and "ts('B',burnArgs" in a
assert 'freshStoppedRpm()' in a
assert 'await storeBackup(pg)' in a and "await api('/api/backup'" in a and "/api/backup/get?page=${page}" in a
assert 'static esp_err_t backup_post(httpd_req_t *r)' in s and 'crc_step(' in s
assert 'Immediate readback mismatch' in a and 'Post-burn mismatch' in a
assert "await api('/api/arm'" in a and "await api('/api/disarm'" in a
print('PASS v4.4 firmware write dispatch, always compiled, persistent backups, and editor interlocks')
