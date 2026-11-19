# Brick Tuner Suite — Nintendo 3DS

This is the reserved location for the actual 3DS / CIA application source. **No 3DS source was included in the uploaded ESP v4.8 archive**, so the directory currently contains documentation only. Do not mistake ESP web UI files for the native 3DS code.

When importing the 3DS project, preserve its build scripts, Makefile, assets and source layout. Its established MSYS2 build command (once the actual project is present) is:

```sh
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

For compatibility, map the 3DS client's network requests to the ESP firmware's status, INI, telemetry and TunerStudio endpoints; do not conflate older v4.8 firmware APIs with later v4.9-v4.12 experimental APIs.
