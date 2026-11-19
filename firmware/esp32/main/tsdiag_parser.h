#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t frame_size;
    uint32_t remaining;
    uint8_t header_bytes;
    uint8_t command;
    uint8_t payload_seen;
    uint8_t in_frame;
} tsdiag_parser_t;

typedef void (*tsdiag_event_cb_t)(uint8_t command, uint64_t timestamp_us, void *ctx);

void tsdiag_init(tsdiag_parser_t *p);
void tsdiag_feed(tsdiag_parser_t *p, const uint8_t *data, size_t len,
                 uint64_t timestamp_us, tsdiag_event_cb_t cb, void *ctx);

#ifdef __cplusplus
}
#endif
