# Brick Tuner Suite v5.9 — Low-memory streamed event audio

## Changes

- Replaced the one-shot event audio buffer (which grew to approximately 470 KiB after adding the three new clips) with two 4,096-frame buffers (16 KiB total) streamed to NDSP channel 0.
- Retained all fourteen original 22,050 Hz mono PCM16 clips, with no SD-card sound files required.
- Recycled a wave buffer only after NDSP reports `NDSP_WBUF_DONE`.
- Kept event sounds higher priority than menu beeps; retained the burn-loop audio and its existing stop/completion behavior.
- Calls `bts_audio_pump()` every application loop to sustain clips longer than one sample buffer.
- Extended **DS Settings → Soundboard** status to show the last real cue, DSP frame count, stream block count, and whether a preview is playing, finished or unavailable.
- Undo now reports whether its cue was queued; when it cannot play it directs the user to the Soundboard diagnostics.
- Unmatched INI menu entries no longer overwrite the last real cue/error status.

## Verification

- Host C syntax check succeeded with the available 3DS stubs.
- `tests/test_v59_streaming_audio.py` compiled and linked all 14 embedded clips and exercised queue/recycle, full playback, burn looping, and stop/restart against a native NDSP mock.
- All 39 `tests/test_*.py` scripts passed, including earlier navigation and writing safeguards.
- `tests/native_mock_write.py` passed all six write-protocol outcomes.
- Uploaded MP3s 11–13 correlate strongly (>0.999) with the bundled PCM audio, and their duration matches.

**Limit:** Real 3DS audio output and installed CIA behavior cannot be verified with the available host tools. The earlier ~470 KiB allocation was a plausible failure point but not established as the actual console root cause. The Soundboard diagnostics now help distinguish loading, cue matching, auto mute and playback progress.

## Build

```
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```
