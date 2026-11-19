# Brick Tuner Suite v5.8 — Missing event sounds fixed

The v5.7 source included Undo controls but no Undo audio trigger, and the three uploaded MP3 sounds were not embedded at all. v5.8 corrects both issues.

- **KidPix I Made a Boo Boo:** plays after a successful local Undo via `edit_history_move(-1)`, whether triggered by touch, X or map tools; not on failed Undo or Redo.
- **Shampoo Is Betta:** plays once when opening a Lambda Display INI setting, matching real labels/targets rather than every render frame.
- **Alex Jones Dracula:** plays when staged cylinder count crosses from 6 or fewer to more than 6; works with both numerical and INI-choice editors.
- All three MP3s were decoded to 22,050 Hz, mono, 16-bit PCM and active-loudness matched to the original 11 embedded clips.
- DS Settings > Soundboard now lists **all 14 sounds** for one-tap or A playback, with no separate SD card audio installation.
- Existing ECU networking, calibration burn verification, and write safeguards are unchanged.

## Verification

- 38 project Python host regression scripts passed individually, including the new test exercising the actual C Undo implementation.
- Root `test_v19_write_contract.py` passed all 15 checks.
- Assembler confirms all 14 PCM files linked and byte ranges match.
- Native audio mock queues all 14 clips through NDSP channel 0 and keeps the shared linear buffer below 500 KB.
- The actual devkitARM CIA build and audio output on a Nintendo 3DS remain unverified here.

## MSYS2 build

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```
