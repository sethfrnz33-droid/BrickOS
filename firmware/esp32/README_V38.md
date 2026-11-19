# BrickOS v3.8 — App navigation, real gauges and custom workspaces

This is a UI/interaction update built directly from v3.7, retaining the native USB CDC protocol, exact-signature firmware INI loading from the STM32 USB SD card (LUN1), Wi-Fi access point `brick`, the fast binary `/api/live` sampler, and the existing staged calibration write protections.

## Application navigation
- Persistent five-section app navigator: **Dashboard**, **Tune**, **Gauges**, **Logs**, and **ECU**.
- On iPhone/narrow mobile widths, the navigator becomes a fixed bottom tab bar, with safe-area spacing.
- The entire firmware tuning tree is **only in Tune**, not permanently alongside the live gauges.
- Tuning lists one firmware system at a time, based on the actual `[Menu]` tree in the currently verified generated INI. Search covers all menu items across every category.
- Open a menu item to show its actual `[UserDefined]` settings or `[TableEditor]` map, with no invented ECU offsets.
- Favorites and recently opened menu items; saved locally in the browser.
- Global **Find screen** command palette in the header. Ctrl/Cmd+K on desktop opens it.
- All maps and all parameters are still accessible from Tune. Existing read, editing, logging and connection tools remain in place.

## Custom cockpit
- Actual semicircular analog dial gauges with moving needles and arcs, plus configurable bar and numeric readouts.
- Live channels are decoded from the INI's output-channel offsets and scale factors; no fake gauge data is generated.
- Tap the settings icon on any gauge to change its channel, style, accent color, display minimum/maximum; move earlier/later or remove.
- Add more instruments (up to 16), reset to defaults, and choose **Gauges + scope**, **Gauge focus**, or **Scope focus** dashboard layouts.
- Select up to three live graph channels; original 10-second USB live graph remains.
- Layout, gauges, graph selections and favorite menus persist per phone/computer using browser localStorage. Different devices will not automatically share settings. Private browsing or data clearing may discard them.
- A channel outside the 768-byte Fast runtime block automatically switches to All-channel capture when selected, rather than silently showing no data.

## Validation
- `node --test tests/*.test.mjs` — 21/21 pass including exact uploaded Proteus F7 INI and raw binary telemetry framing.
- `python tests/v38_browser_smoke.py` — desktop/iPhone view tests with synthetic USB replies, real generated INI, navigation, INI menu searching, command palette, favorites, gauge setup and persistence, data updates, and viewport overflow; PASS with no JavaScript exceptions.
- `bash tests/run_host_syntax.sh` — PASS including SD MSC importer, main C, and telemetry source syntax checks.
- `bash tests/run_assets_host.sh` — PASS; embedded `main/web_assets.c` is regenerated from web/index.html, app.js, core.mjs, app.css.
- The protocol, CDC USB driver, SD reader, INI cache/storage, rusEFI diagnostics parser and associated headers are SHA256-identical to hardware-tested v3.4. `main/main.c` and telemetry.c are identical to v3.7.
- Outdated assertions from older CAN/STA firmware are preserved under `tests/legacy/` and are not valid for this AP-only USB product.

**Still required:** full ESP-IDF 5.3.1 firmware compile on your Windows toolchain and physical testing on ESP32-S3/STM32. High-rate selection specifies a target; actual performance is displayed from the measured sample stream and is not guaranteed.

## Build
Extract the ZIP into its single `BrickOS_v38` directory. In your ESP-IDF 5.3.1 ESP32-S3 environment run `idf.py fullclean build` followed by your standard flash command. Open `http://192.168.4.1/` after joining open AP `brick`.

## Files
- `web/index.html` — application shell
- `web/app.css` — app layout, dials, mobile tabs, tuning browser
- `web/app.js` — UI and live INI-driven controls
- `web/core.mjs` — shared INI and protocol parser, unchanged
- `main/web_assets.c` — auto-generated embedded webpage
- `preview_v38_desktop.png`, `preview_v38_mobile.png`, `preview_v38_tuning.png` — simulated-data browser previews

The separately saved v3.4 golden backup was not modified or replaced.
