# Brick Tuner Suite — DSi edition v1.4

## v1.4 — Exact v62 3DS INI acceptance behavior

- Inspected original 3DS `source/main.c`: `decode_ini()` checks HTTP 200, valid-looking INI body, and a successful `parse_ini_all()`; it **does not compare status and INI signatures** before loading menus or caching the file.
- Removed the extra signature rejection from the DSi `bt_async_poll()` INI parse completion. A completely received, validly parsed INI is now committed to `BrickECU/ecu.ini`, even when the strings reported by `/api/status` and the `[TunerStudio]` signature differ.
- A cached definition can load at startup without a signature-text gate, like 3DS. A change to an already-known ECU signature invalidates the previous definition and triggers a fresh download.
- Signature comparison stays in `signature-debug.txt` **for diagnostics only**, and independent write authorization (including live ECU signature and engine-stopped checks) remains in `bt_commit_field()`. A different signature is no longer mislabeled as an HTTP or INI failure.
- The LINK screen now says `ECU INI: LOADED / PARSED`, never `HASH VERIFIED` based only on a successful parse.
- Local tests include a successful 900KB+ SD download with deliberately different status and INI signature values, alongside normal HTTP/live/timeout/write tests. Native DSi/Unlaunch testing remains outstanding.



## v1.3 — ECU/INI hash diagnostics and correct HTTP 200 reporting

- The ECU signature and INI's `[TunerStudio] signature` are compared by their **complete final decimal layout hash** (6–10 digits). Earlier text, board/branch labels, filename prefixes, and optional `.ini` extensions are not used for compatibility. An actual different hash is still rejected; unknown hashes are not treated as safe.
- The LINK screen shows the ECU and INI hash separately, plus the actual HTTP response code. A downloaded and parsed INI with a wrong hash is a **validation failure after HTTP 200 OK**, not an HTTP transport error.
- On a complete INI parse, the DSi saves `BrickECU/signature-debug.txt` on the mounted SD card with the exact full ECU and INI signatures and the extracted hashes. On a mismatch it preserves the downloaded data as `BrickECU/ecu-unverified.ini` without replacing `ecu.ini`. The unverified INI cannot be used for live decoding or ECU writes.
- Repeated download retries are paused when the hashes differ; press Y on LINK to recheck the ECU and redownload. Other transient transfer errors still retry. The 3DS v62 project and its code remain unchanged.
- Host tests cover different prefixes, wrapped filename-style signatures, missing/truncated/different hashes, >900 KB SD transfers, HTTP 200 diagnostics, protected ECU write checks, and buffer limits. This build has **not** been tested on the user's physical DSi.

## Previous DSi release notes

## v1.1 — Stack/memory fix and truthful transfer stages

- Replaced three 8 KB stack allocations used by parsing `pageIdentifier`, `pageSize`, and `pageChunkWrite` with one 8 KB static scratch block (the parser is single-threaded). This addresses a real DS ARM9 stack-overflow risk on actual ECU INIs; the previous large padding-only host test did not exercise these INI directives.
- INI SD-file parsing reads at most 1024 bytes per frame, reducing stack pressure while remaining responsive. Cached INIs receive the same fix.
- Explicit parser failure diagnostics distinguish heap exhaustion, hard definition limits, and malformed INIs. The project still streams raw INI data to SD and preserves a previously validated cache.
- The UI shows the RX and SD byte counts during transfer and the parser byte count during parsing, rather than presenting the same download count throughout. Unchanged 3DS v62 build.

## v1.0 — SD-buffered ECU INI transfer and 94 KB stall diagnostics

- The DSi client still uses the same BrickOS `/api/ini/file` response as 3DS v62, saving the *complete* raw INI to `BrickECU/ecu.tmp`, parsing it in bounded steps, checking the connected ECU signature, then renaming it to `BrickECU/ecu.ini`.
- TCP receive and FAT storage are **separate cooperative frames**: the incoming packet is buffered in RAM (8 KB queue), then at most 2 KB is written to SD on a subsequent frame. Network reads pause while that queue is drained, avoiding calling `fwrite()` inside DSWiFi's socket receive/HTTP parsing loop. A 4 KB stdio buffer batches FAT writes further.
- The Link screen now displays **RX** (bytes delivered from the ESP32), **SD** (bytes accepted by the open temporary INI file), **PARSE** (bytes re-read from the completed SD file), and the response length when provided. It distinguishes *RECEIVING INI*, *FINISHING SD INI FILE*, and *PARSING INI FROM SD*. This makes a stall around 94 KB diagnosable at the exact phase rather than a single stuck byte number.
- A socket close, incomplete `Content-Length`, failed SD write/flush, unexpected ECU signature, or invalid INI does **not** replace the last verified cache. A failed attempt reports its reason and retries under the existing policy.
- Native hardware still needs verification: the mock server/host suite now transfers an INI **over 900 KB** in HTTP chunks, confirms the byte counters and SD/parse phase order, and verifies that the cached file matches. It cannot reproduce every DSi SD driver or ESP32 timing issue.


**Separate native `.nds` project.** The original Brick Tuner Suite v62 Nintendo 3DS project remains intact in the parent directory. The DSi uses a compact 2D touchscreen layout instead of the 3DS gyro, Citro3D and 3D model effects.

## What works in the source

- Uses the same **BrickOS protocol** as 3DS v62, not a different wireless transfer format. The DSi obtains live ECU status from `GET /api/status`, downloads the installed ECU's complete INI from **`GET /api/ini/file`**, and builds navigation dynamically from a fully received and parsed INI. As in 3DS v62, the INI download is not rejected for a different reported signature text; ECU writes retain separate protection. The source is not hardcoded to a particular engine tune or INI file.
- INI is downloaded using a **cooperative, nonblocking socket**, polled once per VBlank while touch/key input and page rendering remain active. Progress reports actual byte counts from the first received byte, with visible TCP-connect, request, header-wait, and INI-stream phases. No network request can indefinitely hold the input loop; stalled GETs time out after 600 frames without activity (about ten seconds at 60 Hz). The response is **written directly to SD** as `BrickECU/ecu.tmp` in small blocks, supporting HTTP `Content-Length` and `Transfer-Encoding: chunked`. After the complete download, it parses the SD file incrementally across display frames and installs `BrickECU/ecu.ini` without destroying an older cached file on an HTTP, parsing, or SD failure. On subsequent launches the cached INI is parsed in small chunks and reused only if the connected ECU signature matches. No multi-megabyte INI download buffer is allocated. Channel, constant, menu, table and dialog metadata are allocated incrementally rather than reserving ~2.3 MB before startup. Failed downloads retry automatically after a delay; press Y on LINK for manual refresh.
- Nine live gauges default to **the same nine channel names as v62**, with scrolling per-gauge trends. Each slot is reassignable to an actual scalar INI channel; source data is `GET /api/live?length=N&hz=25` with the BLV1 response checked for sequence and size.
- TunerStudio category tree from `[Menu]`, nested groups and dialogs from `[UserDefined]`, real tables and curves from `[TableEditor]` and `[CurveEditor]`, actual live channels and numeric settings.
- Live readback of ECU table cells, optional X/Y axes, and numeric settings via the same `POST /api/ts` `R` requests as v62. Unknown or unsupported definitions are explicitly shown rather than fabricated.
- Numeric map and scalar modifications staged locally, without changing the ECU until explicitly confirmed. The guarded **bench-only** commit path matches the 3DS v62 ESP32 authorization requirements: live ECU and INI signature match, correct page write/burn capabilities, verified backup saved on the ESP32, independent stopped-engine RPM checks, re-read of unchanged baseline, signed `C` chunks with readback, signed `B` burn and post-burn verification, disarm. No automatic write retry on uncertain outcomes.
- Separate LINK page shows association phase, client/gateway IP, BrickOS, ECU session, INI verification and traffic counters. The **expected Wi-Fi SSID is `brick`**, not `Ignotiva S1`. The Wi-Fi library does not expose the currently associated SSID through the API this program uses; `brick` is the expected configured network name, not proof of the actual associated network.

## SD cache and storage diagnostics (v0.9)

The app is built as a `.nds` and needs access to the DSi SD filesystem from the launcher. **The first test is TWiLight Menu++ per-game settings (Y): try `Run in: DSi Mode`, because a DS-mode ROM may have no access to the internal DSi SD card without a working DLDI driver.** This setting is not a guaranteed fix; some DS-only homebrew cannot actually switch modes. If DSi Mode is unavailable, use a launcher that provides functional DLDI access. The app now shows **DS MODE** vs **DSI MODE** on the LINK screen, so the switch can be verified. The host source is DS-mode compatible but may still be run in TWL mode by a supported launcher.

On LINK, press A to mount the filesystem. v0.9 probes the `sd:/` and `fat:/` roots and saves only to a root that actually exists; it never treats the working directory as proof of SD storage. Diagnostic messages distinguish:

- `NO SD DEVICE / TRY DSI MODE`: the filesystem library found no usable `sd:/` or `fat:/` volume; this is a driver/launcher issue, not an ECU or INI issue.
- `SD FOUND / WRITE FAILED`: the volume is readable but creation of `BrickECU/` or a small temporary test file failed.
- `SD READY / BRICKECU`: the app can write to the intended SD volume; only then are cached INIs and new downloads enabled.

A failed `fatInitDefault()` is not assumed to mean the storage is definitely unavailable: the app checks whether a volume is accessible before returning an error. The diagnostic is kept on screen, and Wi-Fi can continue while the SD failure is shown. If the mode is changed, fully restart the game before another test.

- Incoming bytes: `BrickECU/ecu.tmp` on the mounted SD volume.
- After a complete response: re-read from SD incrementally; parse its real menus, fields and maps; check its `signature` against the live BrickOS `/api/status` ECU signature.
- Only then install `BrickECU/ecu.ini`. The old cached file is preserved if download, parsing, or signature verification fails.
- Next launch: parse `ecu.ini` from SD without downloading it again, provided the live ECU signature is the same.
- If the ECU changes: discard old *in-memory* definitions and download the correct new INI into a temporary file. The old valid cached file remains until replacement is verified.

The `.nds` now links `-lfat`, includes `storage.c`, and uses an approximately 9 KB empty definition structure rather than ~2.3 MB of preallocated metadata. The parser expands only the lists actually present in the ECU definition.

## Connection

Use Nintendo DSi System Settings to save an open 2.4 GHz connection to the ESP32 access point **`brick`** in one of the legacy **Nintendo DS Connections 1–3**; this is a DS-mode homebrew build, not a DSi-mode app that supports the enhanced WPA driver. The software expects `192.168.4.1:80`, consistent with the source v62 client. If the network uses another ESP32 IP, modify `BTS_ESP_HOST` in `source/wire.h` to match. Internet connection tests can fail even when local TCP access works.

## Build

In devkitPro MSYS2, install the DS toolchain once with `pacman -S nds-dev`, then:

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi && make clean && make
```

The output is `DSi/Brick-Tuner-Suite-DSi.nds`. Run it through TWiLight Menu++ on DSi. The native devkitARM toolchain was not available for this package's authoring environment; host C syntax checks and transport, INI stream, timeout and protected-write tests passed, but the updated `.nds` build, screen behavior and real ECU transactions **still need physical DSi/ESP32 testing**.

**The original 3DS build command has not changed:**

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe && make clean && make && bash build-cia.sh
```

## DSi controls

| Place | Controls |
|---|---|
| Home | Six touchscreen tabs across the lower screen; L/R choose one of nine gauge slots; A opens TUNE; X MAPS; Y GRAPH |
| TUNE | Up/Down choose ECU INI category; A opens its entries; A opens a map, nested dialog or supported scalar; B back |
| MAPS | Up/Down and A open the calibration readback of a real map |
| MAP/SCALAR edit | D-pad moves cell; X/Y stage +/− changes; L cycles step; R confirms once, R again starts guarded bench-only backup/write/burn; B twice discards uncommitted changes |
| LIVE | Up/Down choose a real runtime channel; X assigns it to the selected gauge; L/R change gauge slot |
| GRAPH | L/R cycle gauge slots, Y shows min/max/mean, X opens channel list |
| LINK | **A** or tap the SD row to mount DSi SD and start cached/dynamic INI loading; X reconnects saved Wi-Fi; Y retests BrickOS and refreshes ECU INI |
| Any top-level screen | SELECT cycles main tabs; START quits |

## Limitations versus 3DS v62

- DSi has 256×192 screens and no 3DS gyro/Citro3D. The graphical style is simplified; 3D gauges, stereo map surfaces, vehicle model, intro and audio soundboard are absent.
- This edition does **not** have full 3DS v62 parity: editable axes, bit-field choice lists/macros, complex INI conditional expressions, 3DS log replay/recording/export, map utilities, autotune, trigger simulator and advanced workflows are not yet ported. Unsupported INI definitions are not silently assumed to be valid.
- The status, INI, and live-data **GETs are nonblocking**, using one socket and strict per-frame receive budgets. SD writes and parsing also use bounded chunks, although individual FAT filesystem calls may briefly occupy a frame. Explicit user-initiated map readback and protected write/backup operations still use synchronous requests and may temporarily stall their individual action; native DSi hardware responsiveness is not yet verified.
- If the INI remains at zero bytes, open LINK to distinguish **TCP CONNECT**, **HTTP REQUEST**, **WAITING HTTP HEADERS**, and errors; ESP AP association alone does not confirm the HTTP endpoint responds.
- **Do not perform unverified ECU writes on a vehicle in service.** This is an untested native port; use a controlled bench setup for validation before allowing any calibration changes.

## Host tests

```bash
cd /c/Users/Seth/Downloads/BrickECU-3DS-Probe/DSi
python tests/run_host_tests.py
python tests/run_write_tests.py
python tests/run_async_tests.py
```

`tests/host_stubs` is only for host compilation and syntax checking; it is not linked into the Nintendo `.nds` build.

## v0.5 DSi Wi-Fi / display repair
- Fixed a leak of DSWifi TCP sockets: all transport cleanup now uses
  `closesocket()` when built for ARM9 (also for abandoned/failing requests).
- Shows underlying socket errno in short Link diagnostics (`E<errno> H<HTTP>`).
  `H0` means no HTTP response was received, not an ESP32 error code.
- Renders both LCD images completely offscreen before transferring them to VRAM,
  preventing the old blank-then-redraw flash during UI and network updates.
- The native DSi binary must still be built and checked on real hardware.

## v0.6 DSi TCP socket compatibility fix
- Fixed the confirmed `NONBLOCK F_GETFL E88 H0` issue. DSWiFi sockets are
  configured with `ioctl(fd, FIONBIO, &enabled)` from DSWiFi's `sys/ioctl.h`,
  not `fcntl(F_GETFL/F_SETFL)` from newlib.
- Removed `select()` on DSWiFi descriptors while TCP is connecting; the
  per-frame nonblocking send attempts the HTTP request only as the connection
  becomes writable, with the existing bounded timeout and socket cleanup.
- Wi-Fi association is not HTTP readiness: if the link is associated but TCP
  still fails, the Link page will now show `SOCKET FIONBIO`, `TCP SEND`,
  `HTTP SEND`, `HTTP RECV` or a response-level error with diagnostic errno.
- The native `.nds` still needs to be built on devkitARM and verified on a
  physical DSi and ESP32. Keep writes off a running engine during testing.

## v0.7: direct SD INI cache + dynamic metadata

- Fixed v0.6 behavior where INI was only processed into a large static RAM structure; no file was ever saved on the DSi SD card.
- Added libfat SD volume probing, temporary download file, checked `fwrite()`/`fclose()`, signature-verified commit and rollback of an older valid cache.
- Download and parse occur in separate cooperative phases so the Link screen distinguishes `SAVING INI TO SD` from `PARSING INI FROM SD`.
- Added startup SD cache replay, retaining the ECU signature match requirement before marking the ECU ready.
- Converted large `BtDefinition` arrays to gradually allocated metadata lists, with unchanged maximum entry caps and fail-closed handling if capacity runs out.
- Host tests include full SD cache persistence, cache replay, wrong-ECU-signature rejection, repeated socket cleanup, chunked HTTP, live telemetry, and protected-write regression checks.
- **Hardware build and actual DSi SD/ESP32 behavior remain unverified.** If `SD: NOT ACCESSIBLE` appears, this is a filesystem/DLDI problem and cannot be fixed by changing the ESP32 IP settings.

## v0.8: diagnose SD black-screen regression

- Restores immediate startup even if the DSi filesystem driver is unsupported: both LCDs are drawn, then Wi-Fi is initialized. SD is not automatically mounted during boot.
- Opens LINK by default with an explicit **A: MOUNT SD AND ENABLE INI** action; if SD initialization hangs on your loader, the screen will already show **MOUNTING SD / WAIT FOR DRIVER**, identifying the failure precisely.
- Until SD is mounted, the ESP HTTP status test still runs, but **no INI download** is attempted and no truncated file is kept in RAM.
- A successful mount opens a previously verified INI cache or resumes the normal download-to-temp-file, signature validation, and cache-commit workflow.
- The blocking DLDI/libfat mount itself cannot safely be made asynchronous with the current DS-mode library; this opt-in diagnostic path avoids trapping users on a black screen as soon as they launch the app.
- This is a startup diagnostic update, not proof that this DSi/nds-bootstrap configuration exposes a writable SD filesystem.

## v1.2: INI name variants

The DSi compares the exact final decimal layout hash component of the ECU
reported signature and the downloaded/cached INI signature. The prefixes
(brand, branch, date, board display name) need not match. Missing, malformed,
or mismatched hashes are still rejected, except that identical nonstandard
signatures retain legacy exact matching. This is NOT a filename comparison.

For guarded bench writes, the ESP32's current full ECU signature is still sent
to its authorization endpoints; it may independently reject the transaction.

Host regression checks: `python3 tests/run_signature_tests.py` and
`python3 tests/run_async_tests.py`. Real hardware verification is outstanding.

## v1.8 – fast two-gauge dashboard and audio

- Two large top-screen readouts default to `AFRValue` and `VBatt`. Tap the LEFT/RIGHT cards on HOME, or use L/R to choose a slot, then press X in LIVE to assign any INI-defined scalar channel.
- Live GET scheduling now aims for a fresh sample every 3 Nintendo VBlanks (20 Hz). The top screen reports **actual completed live responses per second**, not the requested `hz=50` query parameter. This is a target, not a validated hardware guarantee; ESP HTTP errors and socket pressure may reduce the measured rate. The previous E12 resource backoff remains active.
- The v62 3DS event sound library rules are included. Copy all 14 `SD_Files/BrickECU/sfx/*.pcm8` files to `BrickECU/sfx/` on DSi SD. The clips are converted to signed PCM8 mono at 11,025 Hz; they load only when a recognized menu opens or the ECU connection first succeeds. The 3DS sources/assets are unchanged.
- Do not expect a DSi build to use 3DS DSP libraries, Citro3D, gyro support, or streaming DSP buffers.
- This source has host-level tests but still needs devkitARM compilation and real DSi/ESP32 verification, especially for sustained 20 Hz and concurrent sound.
