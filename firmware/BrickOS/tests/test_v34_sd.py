from pathlib import Path
p=Path(__file__).resolve().parents[1]
s=(p/'main/usb_sd_ini.c').read_text()
v=(p/'main/vcp_tcp_gateway.cpp').read_text()
assert 'st.st_size > 1200000' in s
assert 's_candidates' in s and 's_last_file' in s
assert 'format_if_mount_failed = false' in s
assert 'brick_sd_ini_host_install()' in v
assert v.index('cdc_acm_host_install(') < v.index('brick_sd_ini_host_install()')
assert '.lun = 1' in (p/'CMakeLists.txt').read_text()
print('PASS: SD LUN1 reader, generated INI size, CDC-first startup, diagnostics')
