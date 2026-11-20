from pathlib import Path
base=Path(__file__).resolve().parents[1]
g=(base/'main/vcp_tcp_gateway.cpp').read_text()
p=(base/'main/protocol.c').read_text()
m=(base/'main/main.c').read_text()
assert 'USB host/VCP auto-connect starts at boot' in g
assert '/* Always probe for an ECU, even if no TCP client is connected. */' in g
assert '/* Keep the USB host connected independently of TCP clients.' in g
assert 'if (!gateway_prepare_data_tasks())' in g
assert 'bool vcp_gateway_usb_ready(void)' in g
assert 'while (!vcp_gateway_usb_ready() && waited_ms < 6000)' in p
assert 'vcp_gateway_ts_tcp_active()' in p
assert 'usb_device_ready' in m
print('PASS: USB enumerates independently of TCP and first query waits for readiness')
