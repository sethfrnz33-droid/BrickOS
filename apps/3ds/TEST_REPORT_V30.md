# Brick Tuner Suite v3.0 validation

## Passed in this container

- Host GCC syntax check of `source/main.c` using the included 3DS compatibility stubs.
- Screen routing and layout contracts: upper gauges by default; dedicated maps/graphs override gauges; 5 digital layouts; 4-row navigation; semantic icons; no Pikmin mesh renderer in the build.
- Native icon classification checks for 13 ECU-system names.
- Compiled navigation hierarchy, folder-first ordering, and bookmarked return-position tests.
- Live gauges preserve their last valid values through network dropouts.
- rusEFI fault message parsing and field-shortcut tests.
- INI auto-sync and trigger preflight request simulations.
- Six ECU calibration write/flash/readback mock scenarios, including five failures.
- Native trigger illustration and L+R safety-dialog input regression tests.

## Not tested here

- Full devkitARM/tex3ds build into `.cia` (3DS toolchain unavailable).
- Rendering on physical 3DS LCD, actual framerate, touch feel, or connection to physical ECU hardware.

The `V30_LAYOUT_PREVIEW.png` is a programmatic **layout mockup**, not a console screenshot or hardware test.
