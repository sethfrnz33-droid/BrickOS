#include "tsdiag_parser.h"

static void finish_frame(tsdiag_parser_t *p, uint64_t timestamp_us,
                         tsdiag_event_cb_t cb, void *ctx)
{
    if (cb)
        cb(p->command, timestamp_us, ctx);

    p->frame_size = 0;
    p->remaining = 0;
    p->header_bytes = 0;
    p->command = 0;
    p->payload_seen = 0;
    p->in_frame = 0;
}

extern "C" void tsdiag_init(tsdiag_parser_t *p)
{
    if (!p) return;
    p->frame_size = 0;
    p->remaining = 0;
    p->header_bytes = 0;
    p->command = 0;
    p->payload_seen = 0;
    p->in_frame = 0;
}

extern "C" void tsdiag_feed(tsdiag_parser_t *p, const uint8_t *data, size_t len,
                             uint64_t timestamp_us, tsdiag_event_cb_t cb, void *ctx)
{
    if (!p || !data)
        return;

    for (size_t i = 0; i < len; ++i) {
        const uint8_t b = data[i];

        /*
         * TunerStudio may use the legacy one-byte H/O query before
         * switching to the msEnvelope_1.0 framed protocol. Count those
         * commands directly so they cannot poison the framed parser.
         */
        if (!p->in_frame && p->header_bytes == 0 &&
            (b == (uint8_t)'H' || b == (uint8_t)'O')) {
            if (cb)
                cb(b, timestamp_us, ctx);
            continue;
        }

        if (!p->in_frame) {
            if (p->header_bytes == 0) {
                p->frame_size = (uint16_t)b << 8;
                p->header_bytes = 1;
                continue;
            }

            p->frame_size |= b;
            p->header_bytes = 2;

            if (p->frame_size == 0) {
                p->frame_size = 0;
                p->header_bytes = 0;
                continue;
            }

            p->remaining = (uint32_t)p->frame_size + 4U;
            p->command = 0;
            p->payload_seen = 0;
            p->in_frame = 1;
            continue;
        }

        if (!p->payload_seen) {
            p->command = b;
            p->payload_seen = 1;
        }

        if (p->remaining > 0)
            p->remaining--;

        if (p->remaining == 0)
            finish_frame(p, timestamp_us, cb, ctx);
    }
}
