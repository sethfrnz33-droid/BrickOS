# Brick Tuner Suite v5.5 — Event audio fix verification

- Root cause found: in v5.4, every event clip was gated by the persisted legacy `sfx` (menu-beeps) preference. Users who had turned off menu beeps could not hear any event sounds.
- Fixed: event clips use an independently saved `event_audio` setting, enabled by default even when `sfx=0`.
- Initialized event audio independently of optional menu-beep sample allocation.
- Added DS Settings > Event Sounds / Preview, showing NDSP initialization result, actual loaded sample count, and manual cue previews; X toggles event audio without changing menu beeps.
- Strengthened PCM buffering with 128-byte padding and channel initialization for hardware compatibility.

## Checks completed

- 33/33 host Python regression scripts passed in two separate batches.
- `tests/test_v55_audio_output.py` compiled and linked the actual 11 embedded PCM files, compared every copied sample against the embedded bytes and queued all 11 through a simulated NDSP channel.
- All 6 native ECU mock-write protocol scenarios passed. No engine control or safety checks were changed.

## Not verified

- Actual devkitARM compile, hardware NDSP init and audible playback on a 3DS. If playback still fails, the sound preview shows DSP status and loaded samples to distinguish an initialization failure from an event-trigger issue.
