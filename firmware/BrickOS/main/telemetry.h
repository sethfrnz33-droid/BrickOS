#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

// USB CDC is unchanged: this module only schedules safe read-only 'O' requests.
#define BRICK_LIVE_MAX_BYTES 3072

typedef struct {
    uint32_t sequence;
    uint32_t sample_ms;
    uint32_t sample_duration_us;
    uint16_t length;
    uint16_t target_hz;
    uint8_t bytes[BRICK_LIVE_MAX_BYTES];
} brick_live_snapshot_t;

typedef struct {
    uint32_t sequence;
    uint32_t samples_ok;
    uint32_t samples_failed;
    uint32_t age_ms;
    uint32_t sample_duration_us;
    uint32_t actual_millihz;
    uint16_t requested_hz;
    uint16_t bytes_per_sample;
    bool active;
} brick_live_stats_t;

esp_err_t brick_live_init(void);
void brick_live_subscribe(uint16_t length, uint16_t hz);
// Suppress gauge polling briefly while the calibration editor owns the USB link.
void brick_live_holdoff(uint32_t milliseconds);
bool brick_live_copy(brick_live_snapshot_t *out);
void brick_live_get_stats(brick_live_stats_t *out);
