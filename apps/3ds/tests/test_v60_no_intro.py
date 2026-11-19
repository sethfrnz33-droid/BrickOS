#!/usr/bin/env python3
"""The splash was intentionally removed; HOME Menu 3D banner is unaffected."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'source/main.c').read_text()
mk=(root/'Makefile').read_text()
assert 'home_startup' not in s
assert 'BTS_INTRO_MS' not in s
assert 'intro_playing' not in s
assert 'intro_smoke' not in s
assert 'boot_started' not in s
assert 'intro_car' not in mk and 'intro_fx' not in mk
assert not (root/'art/intro_car').exists()
assert not (root/'art/intro_fx').exists()
assert 'else home_dashboard();' in s
assert 'draw_fault_top_overlay();' in s
assert 'draw_power_cycle_top();' in s
assert 'if(!intro_playing' not in s
assert (root/'home-banner/build_3d_banner.sh').exists()
print('PASS: app starts directly on the dashboard; no splash assets or input lock; HOME Menu banner retained')
