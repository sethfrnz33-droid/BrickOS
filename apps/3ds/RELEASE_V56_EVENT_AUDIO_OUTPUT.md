# Brick Tuner Suite v5.6 — Event sound output correction

## What actually changed
- The old normal interface beeps use NDSP channel 0; the v5.4 and v5.5 clips used a separate channel 1.
- The new event player uses channel 0, which has already been proven to produce sound on the user's console.
- Short menu beeps no longer interrupt a queued event clip; a new event may replace a previous event.
- A single approximately 387 KiB linear audio buffer is reused for all 11 embedded clips rather than keeping approximately 1.17 MB of clip allocations alive simultaneously.
- The sound-test page now reports the real embedded-clip availability, memory readiness, request/queue counters, last requested clip, and NDSP wave status.
- No SD-card audio folder or audio conversion is required; original event rules and all ECU write safeguards remain unchanged.

## Root-cause qualification
The user confirmed that normal menu beeps were audible and that the Menu Sound switch was ON. Consequently, muting was **not** the explanation for the v5.4 failure. Separate channel and allocation paths were found and replaced, but the exact on-device cause remains unconfirmed until hardware playback is retested.

## Testing
Host regression tests and native mock audio tests check embedded asset assembly, all 11 sample bytes, single capped allocation, NDSP output queueing, and event-rule transitions. A real Nintendo 3DS playback check is still required.
