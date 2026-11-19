> **SUPERSEDED BY v5.4:** All 11 sounds are now embedded directly in the compiled application. No MP3 conversion, SD sound folder, or copy step is required. The instructions below document the original v5.3 implementation only.

# Brick Tuner Suite v5.3 — Conditional 3DS Audio

All sound events use the user-provided MP3s, loaded at runtime **from the 3DS SD card** in WAV format. Audio clips are NOT redistributed inside the source ZIP. Prepare your existing files locally with `tools/prepare_sounds.py` and copy the result to the SD card.

## Sound preparation (one time)

With Python 3 and FFmpeg installed, from the project directory:

```sh
python tools/prepare_sounds.py "C:/Users/Seth/Downloads"
```

This outputs `sdcard/BrickECU/sounds/*.wav`. Copy the generated `BrickECU/sounds` folder to the **root of your 3DS SD card**. The app uses `sdmc:/BrickECU/sounds/`.

Audio is decoded **only when loading**: uncompressed 22,050 Hz, 16-bit little-endian, mono WAV. NDSP channel 1 plays clips; legacy menu beeps use channel 0. If files are missing or DSP is unavailable, the tuner continues silently without interrupting ECU operations.

## Conditional sounds

| Sound filename | Trigger |
|---|---|
| `launch.wav` | Open Launch Control settings |
| `vvl.wav` | Open VVL Control settings |
| `four_cyl_honda.wav` | Select the 4-cylinder value in an INI cylinder-count field, or a Honda-named option in an INI trigger-pattern field |
| `config_error.wav` | New **confirmed** ECU configuration fault, never a transient HTTP error |
| `boost_nitrous.wav` | Actual fresh, decoded manifold pressure rises above **100 kPa** (rearms below 98 kPa); or opening Nitrous/Rotational Idle settings |
| `limiter_antilag.wav` | Open Limiter or Anti-Lag settings |
| `unsaved.wav` | Confirm discarding changed local calibration bytes without burning |
| `burning.wav` | The **actual ECU flash phase** begins (not merely a queued write or rejected preflight) |
| `burn_verified.wav` | ECU burn ACK and **independent post-burn readback verification** succeed |
| `connected.wav` | New confirmed ECU live session |
| `coolant_hot.wav` | Fresh coolant reading **above 105°C**, repeating every 5 seconds while still above; stops as soon as it cools |

The clip engine triggers once per opening/selection/verified transaction, not every draw frame. MAP and coolant events require valid INI-decoded live telemetry with recognized units. It does not alter the ECU configuration or suppress existing fault and power-cycle prompts. DS Settings → Menu Sound ON/OFF controls all optional effects.

## Validation

Host compilation and transition tests check event rules and handler placement. **Actual 3DS audio playback, timing, and speaker volume remain unverified.** The sound files must be installed on the SD card for them to play.
