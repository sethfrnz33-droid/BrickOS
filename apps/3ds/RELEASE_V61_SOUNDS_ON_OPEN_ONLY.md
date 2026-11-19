# Brick Tuner Suite v6.1 — Event cues only on open

- The Tune lower-screen preview no longer fires Lambda, Cylinder, or other context audio when its cursor moves with D-pad, Circle Pad, or C-stick.
- Touching a different preview row changes selection silently; touching an already selected row opens it and can play the corresponding cue.
- Opening an INI subfolder or setting still triggers its contextual cue with the existing cooldown.
- Incrementing/decrementing map or scalar values no longer replays the contextual *opening* cue. Explicit enumerated and numeric choice-change events, including four-cylinder, Honda-pattern, and cylinder-count choices, remain wired.
- Other sounds, event audio assets, ECU operations and UI remain unchanged.

## Verification
Host C syntax and regression scripts tested. Real console audio behavior requires on-device verification.
