# Brick Tuner Suite v5.7 — Error navigation, reliable burn audio, soundboard

This release builds from v5.6 and preserves the working 3DS audio channel 0 backend.

## Fault handling
- Refresh the real ECU config error after bridge/ECU reconnection; successful `OK`, `0`, or empty ECU replies clear an existing alert following the validation rules. Failed replies never clear a real fault.
- Open exact matching INI fields directly; otherwise provide related **real INI menu** links or a related tuning category. Do not fabricate ECU fields.

## Audio
- The 11 supplied clips remain embedded and are normalized to similar active loudness, peak limited to avoid clipping.
- At the confirmed burn request, Nom Nom Nom loops through preflight, transfer, flash, and verification; it stops before the XP cue on verified success or Fart With Reverb on any failure. A canceled queued burn also stops the loop.
- DS Settings -> Soundboard / Event Sounds gives direct one-touch or A-button playback for all 11 clips; Y stops a preview, X toggles automatic event-trigger playback, and B returns. Preview works independently of the automatic-sounds toggle. No audio files need copying to SD.

## Editing UI
- Upper-screen Tune gauges no longer have accent bars crossing their readings; values and labels are horizontally centered.
- A visible Undo / Redo row is available on scalar and curve editors. The 2D-map editor retains its Undo control; X/Y shortcuts provide Undo/Redo on choice lists. Undo only affects staged local edits, not ECU flash.

## Pending user-supplied audio
Three additional named clips were requested but not included in the uploaded files: **Shampoo Is Betta**, **KidPix I Made a Boo Boo**, and **Alex Jones Drac**. They have NOT been silently substituted or marked as installed; their triggers remain pending the original audio clips.

## Verification
Host syntax checks and regression tests were run in this container; actual devkitARM compilation, physical ECU reconnection clearing, audio volume and playback, and burn timing on a 3DS remain unverified.
