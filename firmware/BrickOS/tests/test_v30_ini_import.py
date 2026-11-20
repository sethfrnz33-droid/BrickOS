from pathlib import Path
root=Path(__file__).resolve().parents[1]
js=(root/'web/app.js').read_text()
sd=(root/'main/usb_sd_ini.c').read_text()
assert 'accept=".ini,.txt,text/plain,application/octet-stream"' in js
assert 'ini-import-progress' in js and 'uploadIniWithProgress' in js
assert 'request.upload.onprogress' in js and 'Verifying cached INI' in js
assert 'f.size>1200000' in js
assert 'USB SD auto-import disabled' in sd
print('PASS: INI picker, progress, cache verification, and SD error states')
