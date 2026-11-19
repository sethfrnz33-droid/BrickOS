# Brick Tuner Suite v6.0 — Fault/session, sounds, log deletion, instant startup

- Remove the former E46 smoke/donut intro from the app executable and build.
  The existing 3D HOME Menu car banner is not changed.
- Clear a previous-session error automatically after the ECU is *confirmed* disconnected and reconnected; request fresh error data immediately. Do not suppress current ECU errors.
- Trigger Lambda/Cylinder sound cues by their keywords on deliberate setting interactions, with a cooldown.
- Add confirm-before-delete for app-created CSV logs, remove companion metadata and play KidPix only on a successful delete.
- Replace the looping burn soundtrack with the user-provided Jeopardy clip; interrupt for verified XP-success or failure cue.
- Increase and limit embedded clip volume to make playback more even and less quiet.

Verification: host-side build and regression tests only. ECU and real Nintendo 3DS hardware unverified.
