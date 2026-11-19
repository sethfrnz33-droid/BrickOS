# Brick Tuner Suite v4.5 — readable UI / restrained stereo / faster write planning

Based on v4.4. This is a complete source project, not a prebuilt CIA.

## Changes

- Reduced the separated stereoscopic text layer. Selected Tune cards still project forward, but their icons and letters stay approximately on the same plane as the card. Dashboard gauges retain their forward projection; Tune-side gauges retain their recessed position.
- A selected cell in the 2D table editor now gets a dedicated foreground pass, with small stereoscopic disparity, matching label, visible outline and slight vertical lift. No calibration values are changed by the drawing code.
- Minimum legible font scale for UI `text()` labels is now 0.43; overly long labels are ellipsized to stay within the active top/bottom screen width without shrinking to tiny text. UTF-8 truncation keeps codepoint boundaries intact. Cached map labels show up to a 6-column by 5-row viewport, with larger cell and axis font starting scales.
- The burn worker uses a bounded 128-byte write planner that combines nearby changed bytes (at most 12 bytes of unchanged baseline between changes) to reduce separate HTTP write/readback requests for scattered edits. It only runs after the original ECU bytes have been freshly read and matched. The page backup, independent zero-RPM checks, signature checks, arm/disarm, per-block acknowledgement/readback, once-per-page flash burn, and post-burn readback all remain intact.
- Successful burns display elapsed seconds plus preflight/backup and flash timing so real-hardware bottlenecks can be identified.

## Testing

- 23/23 project Python regression scripts passed, including native C burn-planner, stereo, map, and error checks.
- 6/6 native mocked ECU protocol scenarios passed (success; backup unavailable; nonzero RPM; baseline mismatch; write rejected; burn rejected).
- Host GCC syntax check passed with existing host-only library stub warning. devkitARM compilation, full CIA packaging, on-console typography/stereo, actual ECU write time and real flash timing were **not** tested here.

## Build

In MSYS2, inside your updated project folder:

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

**Safety:** A positive ECU acknowledgement is not proof that an engine tune is safe. Only use the bench write capability when the engine is stopped, and always verify the resulting calibration.
