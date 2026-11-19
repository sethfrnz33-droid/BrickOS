# Brick Tuner Suite v5.4 — self-contained conditional sounds

- Converted all 11 supplied MP3 clips here with ffmpeg to 22,050 Hz, 16-bit mono PCM.
- Embedded all converted samples directly into the ELF/3DSX/CIA using `source/event_audio_assets.S` and an explicit Makefile object.
- Changed event playback to load those samples into NDSP-accessible linear memory at startup, instead of reading `/BrickECU/sounds/*.wav` from the SD card.
- No separate conversion step, WAV installation, or additional SD files are needed. Existing tuning menus and event trigger logic are unchanged.
- Sound Enable/Disable still works; visual warnings and ECU burn safeguards are untouched.
- Adds about 1.17 MB to the executable and uses up to 1.17 MB of linear RAM for samples.

## Verification

Converted all 11 files and validated the raw sample sizes and format. Real 3DS audio playback and CIA build are not verified in this environment.
