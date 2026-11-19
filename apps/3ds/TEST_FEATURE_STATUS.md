# Brick Tuner Suite v6.3 — imported source and test-feature status

Source: user-provided `Brick-Tuner-Suite-v63-3D-MAP-CAMERA-FIX(1).zip` (2026-11-19).
This `apps/3ds/` directory contains the **native 3DS source project**, not the DSi port.

## Current verification

- 42 of 42 included Python host regression scripts passed when run against the extracted archive (2026-11-19). This includes code-contract checks and mock compiles; **not** physical 3DS or live ECU verification.
- v6.3's 3D map camera/polar projection change is covered by `tests/test_v63_polar_map_projection.py`.
- No devkitPro `3ds-dev` or real ECU connection was available in the import environment; a native `.3dsx` / `.cia` build and live behavior remain unverified.

## Test feature inventory (actual source)

| Feature | Current source behavior | Status |
|---|---|---|
| Offline trigger waveform viewer | Draws and animates local trigger wheel at user-selected preview RPM; does not change ECU pins | Implemented offline preview |
| Internal trigger stimulator | 3DS requests bridge preflight/start/stop and uses a session-bound confirmation; bridge independently enforces its policy and stop watchdog | Source integration and host checks only; not a claim about physical outputs |
| Generic INI command buttons | INI commandButton entries parse, but only self-stimulation Start/Stop have supported 3DS handlers; other physical test commands are explicitly unavailable | Partial |
| Throttle/DBW actuator test | No verified physical throttle test command handler or closed-loop test controller | Not implemented |
| PID autotuning | No general verified PID tuning algorithm/transactional controller available in this 3DS release | Not implemented |

## Build command (Windows devkitPro MSYS2)

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

## Integration policy

Keep `apps/3ds/`, `apps/dsi/`, and `firmware/esp32/` separate; future features should be validated against the actual ECU/INI version before being labeled working. Do not mistake local simulation for live ECU actuation. ECU output testing requires independent hardware safety validation and is intentionally not enabled by generic INI commands.
