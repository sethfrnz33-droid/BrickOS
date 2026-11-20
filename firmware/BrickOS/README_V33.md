# BrickOS v3.3 — Run ECU query reset isolation

- AP-only `brick` access point; no INI downloads/STA mode.
- Working CDC enumeration and signature handshake are retained.
- Automatic SD/MSC reader stays disabled, as in v3.2.
- Moves the request argument buffer, reply buffer, and USB transport frames from FreeRTOS task stacks to heap; checks allocation failures.
- HTTP task stack enlarged from 10240 to 16384 bytes.
- `/api/status` now provides `query_count`, `last_query_stage`, `last_query_stage_number`, and `http_stack_free_words`.
- Query stages persist across software/watchdog resets (warm reset) for crash localization.
- `last_query_stage` values: HTTP_QUERY_ENTERED, USB_PROTOCOL_REQUEST, USB_PROTOCOL_RETURNED, HTTP_RESPONSE_SENT.

**Hardware test:** flash, reconnect phone to `brick`, run ECU query, then inspect `/api/status` for `boot_counter`, `reset_reason`, `last_query_stage`, `http_stack_free_words`. If reset persists, a UART panic backtrace is needed for precise cause. This is a reasoned stability fix, not hardware verified.
