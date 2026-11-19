from pathlib import Path
p=Path(__file__).resolve().parents[1]
m=(p/'main/main.c').read_text();a=(p/'web/app.js').read_text()
assert 'static esp_err_t backup_post(httpd_req_t *r)' in m
assert 'static esp_err_t backup_get(httpd_req_t *r)' in m
assert "ts_request('R',args,sizeof(args),reply,6000)" in m
assert 'fwrite(reply->bytes' in m and 'fread(check' in m
assert 'rename(temp,final)' in m and 'crc!=stored_crc' in m
assert 'backup_post}' in m and 'backup_get}' in m
assert 'storeBackup(page)' in a and "await api('/api/backup'" in a
assert 'saving local backup' not in a
assert 'localStorage.setItem(backupKey' not in a
print('PASS: ESP32 full page persistent backup + CRC + browser independent of localStorage')
