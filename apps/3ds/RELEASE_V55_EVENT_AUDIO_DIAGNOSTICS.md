# Brick Tuner Suite v5.5 — Event audio playback and diagnostics

- Fixed a genuine v5.4 control bug: all 11 event sounds were gated by the saved `sfx=0` option for legacy menu beeps. Event sounds now have a separate `event_audio` setting, enabled by default.
- The NDSP service initializes event sound clips even if optional menu-beep allocation fails.
- A new **DS Settings > Event Sounds / Preview** page exposes NDSP initialization result, the number of loaded samples, and a selectable preview of every bundled sound. A queues the selected sound, X toggles future automatic event sounds, B returns.
- Added padded linear PCM buffers and explicit NDSP channel reset/queue setup for reliable reuse.
- All audio remains embedded in the build; no MP3 conversion or SD card copying is required.
- Existing ECU safety checks and tuning paths unchanged.

## Limitations

Without a real devkitARM/3DS audio device, audible playback cannot be verified in this environment. Preview reports a cue **queued** to NDSP, not proof of sound coming from speakers. If DSP initialization fails, the sound page displays its actual error code and loaded sample count.
