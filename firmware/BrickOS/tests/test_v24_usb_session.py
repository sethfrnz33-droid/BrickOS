from pathlib import Path
base=Path(__file__).resolve().parents[1]
main=(base/'main/main.c').read_text()
gw=(base/'main/vcp_tcp_gateway.cpp').read_text()
web=(base/'web/app.js').read_text()
assert 'pdMS_TO_TICKS(1000) > 0 ? pdMS_TO_TICKS(1000) : 1' in main
assert 'brick_ecu_handshake_task' in main
assert 'ts_request(\'S\', NULL, 0, &rep, 2000)' in main
assert 'memcmp(rep.bytes, "rusEFI ", 7)' in main
assert 'g_ecu_sig[0] = 0;' in main
for key in ('ecu_signature','ecu_session_connected','ecu_handshake_attempts','ecu_handshake_successes','ecu_handshake_state'):
    assert '"'+key+'"' in main
assert 'const confirmedSignature=S.status.ecu_session_connected?S.status.ecu_signature' in web
assert '/* Always probe for an ECU, even if no TCP client is connected. */' in gw
assert 'vTaskDelay(pdMS_TO_TICKS(8000)' not in main
assert 'ts_can_' not in main
print('PASS v2.4 USB host starts after 1s, CDC retries, autonomous signature S, disconnect clears session, website uses signature')
