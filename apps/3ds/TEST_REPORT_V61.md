# v6.1 regression results

- **40/40** host Python regression scripts passed, including `test_v61_event_sounds_open_only.py`.
- The updated navigation removes sound dispatch from D-pad/C-stick selection changes and touch focus changes, but retains event cues when an INI entry or subfolder actually opens.
- Normal ECU/sound triggers, soundboard and burn audio are unchanged.
- Native 3DS audio/UI still requires device verification.
