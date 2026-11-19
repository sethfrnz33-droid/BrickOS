# External references, source checks and assumptions

Checked on 2026-11-18 (UTC/Canada Pacific date):

- rusEFI exact INI lookup and signatures: https://wiki.rusefi.com/INI-lookup-logic/
- rusEFI generated config: https://wiki.rusefi.com/Config-Generation/
- rusEFI integration configuration source: https://github.com/rusefi/rusefi/blob/master/firmware/integration/rusefi_config.txt
- rusEFI output channel definitions: https://github.com/rusefi/rusefi/blob/master/firmware/console/binary/output_channels.txt
- rusEFI idle control: https://wiki.rusefi.com/Idle-Control/
- rusEFI first-start guide: https://wiki.rusefi.com/HOWTO-Start-An-Engine/
- rusEFI cranking: https://wiki.rusefi.com/Cranking/
- rusEFI fuel flow/model: https://github.com/rusefi/rusefi/blob/master/docs/AI/fueling_system.md
- rusEFI ignition design: https://github.com/rusefi/rusefi/blob/master/docs/AI/ignition_system.md
- rusEFI TunerStudio guide: https://wiki.rusefi.com/Get-tuning-with-TunerStudio-and-your-rusEFI/
- rusEFI flash behavior: https://wiki.rusefi.com/HOWTO-quick-start/
- GitHub repository creation: https://docs.github.com/en/repositories/creating-and-managing-repositories/creating-a-new-repository

These are source references, **not** warranties that every rusEFI board and firmware revision exposes every described feature. Generated INI layout and active configuration are the authoritative device-specific input. Verify exact firmware hashes and functions before implementation. Some proposals are original architecture recommendations, not documented rusEFI features.

## Local project materials used

User-provided project `BrickOS-v412-SmartSpark-Motion-Evidence.zip`, specifically: `README_V49_AUTOTUNE.md`, `README_V411_TUNE_GUIDE.md`, `README_V412_MOTION_EVIDENCE.md` and C module headers `autotune.h`, `fuel_trims.h`, `tune_guide_core.h`, `motion_evidence_core.h`. These establish the v4.12 baseline, not complete implementation of this plan.

## Unknowns requiring investigation

- How widely current exact INI fingerprints vary across firmware families and what versioned semantic aliases are required.
- The actual measured ECU/ESP USB telemetry and write throughput under load.
- Clock synchronization fidelity from 3DS IMU to ECU signals.
- Bank-wise trim interpretation across specific firmware layouts.
- Which sensor/actuator diagnostic signals are exposed for every supported board.
- ESP storage performance and endurance for persistent evidence logging.

Resolve these with fixtures, device observations and tests, not invented constants.
