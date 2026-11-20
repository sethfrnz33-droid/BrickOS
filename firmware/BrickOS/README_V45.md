# BrickOS v4.5 — One-tap Burn + visible status

## Change log
- Replaces the old Apply/write/burn choices in the phone tuner with one **Burn changes** button, which commits all currently staged calibration fields across their pages.
- While burning, a status dialog shows real transaction stages (Backup, Compare, Write, Flash, Verify), the current page/offset and measured elapsed seconds. It does not invent ECU progress percentages or offer an ETA. On verified success the dialog closes automatically; on failure it stays visible with the exact failing phase, and does not silently retry a write or burn.
- When you navigate away from a setting with unburned changes, a cancelable warning explains that the values are staged locally and are not yet saved on the ECU. The Tune tab shows a pending-edits badge, and refresh/close of the page triggers a standard unsaved-changes warning where the browser supports it. Staged edits are retained in the open browser session if you navigate away.
- Scalar and enumeration settings now stage as you edit, without a separate Stage button. The always-visible Burn dock shows pending fields/bytes and an explicit NOT SAVED label.
- The previous unconditional 650 ms post-flash sleep is removed. BrickOS attempts readback as soon as the ECU acknowledges B, waits only if data has not settled, and performs up to three short, read-only retries (160, 350, 700 ms). A B command is never retried because its outcome may be uncertain. Actual ECU flash/USB time is hardware-dependent and cannot be honestly predicted from a browser simulation.
- Full-page ESP32-side backup, CRC/length verification, pre-write stale-data detection, direct zero-RPM check, signature check, short-lived authorization, post-write checks and burn readback verification are retained.

## Build and validation
This ZIP contains **complete ESP-IDF source**, NOT a preflashed ESP32 binary. Build and flash the project and check `http://192.168.4.1/api/status` returns `brick_os_build: "v4.5"`. Refresh the browser to load the new interface. Test only on a stationary, stopped-engine bench ECU.

Automated tests: `node --test tests/*.test.mjs`, `python3 tests/v45_browser_smoke.py`, `python3 tests/v45_burn_ux.py`, `sh tests/run_host_syntax.sh`. Chromium tests use a fake ECU backend and do not prove live hardware speed or flash persistence.

## Performance caveat
Firmware must still save a *fresh complete calibration-page backup* on the ESP32 before it transmits ECU changes. On real hardware that can dominate transaction time. v4.5 specifically removes the previous fixed 650 ms delay; it does **not** bypass this backup or promise any measured speedup on the physical ECU.
