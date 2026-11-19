from pathlib import Path
p=Path(__file__).resolve().parents[1]
main=(p/'main/main.c').read_text()
protocol=(p/'main/protocol.c').read_text()
assert 'cfg.stack_size=16384;' in main
assert 'RTC_NOINIT_ATTR static uint32_t g_query_stage;' in main
assert '"last_query_stage"' in main
assert 'g_query_stage = 2;' in main and 'g_query_stage = 3;' in main
assert 'uint8_t *bytes = (uint8_t *)malloc(TS_MAX_DATA);' in main
assert 'ts_reply_t *reply = (ts_reply_t *)calloc(1, sizeof(ts_reply_t));' in main
assert 'uint8_t *req = (uint8_t *)malloc(TS_MAX_DATA + 6);' in protocol
assert 'uint8_t *resp = (uint8_t *)malloc(TS_MAX_DATA + 5);' in protocol
assert 'free(req);' in protocol and 'free(resp);' in protocol
assert 'brick_sd_ini_start();' not in main
assert 'WIFI_MODE_APSTA' not in main
print('PASS v3.3: heap USB/query buffers, HTTP stack, reset breadcrumbs, AP-only, MSC disabled')
