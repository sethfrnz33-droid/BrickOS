# v6.0 host verification

- 39/39 Python regression scripts passed, including native tests for ECU session fault invalidation, safe CSV log deletion and confirmation, 14 embedded clips, loudness normalization and streamed Jeopardy burn audio.
- 6/6 simulated ECU protocol write scenarios passed (success, backup failure, running RPM rejection, baseline conflict, rejected write, rejected burn).
- App opens directly to the normal dashboard: smoke/donut intro, input lock and obsolete sprite assets removed.
- The Nintendo 3DS HOME Menu E46 3D banner remains unchanged.
- No devkitARM/CIA build or hardware audio/ECU test was run in this container. Real hardware behavior remains unverified.
