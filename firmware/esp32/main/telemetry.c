/* BrickOS read-only telemetry collector.
 * Session and USB-CDC driver remain owned by protocol.c/vcp_tcp_gateway.cpp.
 * One shared sample serves every browser poll without repeating the ECU query.
 */
#include "telemetry.h"
#include "protocol.h"
#include "vcp_tcp_gateway.h"
#include <stdlib.h>
#include <string.h>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

extern void brick_ecu_signature_get(char *dst, size_t cap);

static SemaphoreHandle_t s_mutex;
static brick_live_snapshot_t s_latest;
static uint32_t s_ok, s_failed, s_actual_millihz, s_window_samples;
static uint32_t s_window_start_ms;
static int64_t s_interest_us;
static uint16_t s_hz = 20;
static uint16_t s_length = 512;
static bool s_valid;
static int64_t s_holdoff_until_us;

static uint32_t millis_now(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
static TickType_t ticks_at_least_one(uint32_t ms) {
    TickType_t ticks = pdMS_TO_TICKS(ms);
    return ticks ? ticks : 1;
}

// Browser interest expires after four seconds; idle browser never monopolizes USB.
void brick_live_subscribe(uint16_t length, uint16_t hz) {
    if (!s_mutex) return;
    if (length < 4 || length > BRICK_LIVE_MAX_BYTES) return;
    if (hz != 10 && hz != 20 && hz != 50 && hz != 100) return;
    if (xSemaphoreTake(s_mutex, ticks_at_least_one(100)) != pdTRUE) return;
    const int64_t now = esp_timer_get_time();
    if (s_interest_us <= 0 || now - s_interest_us >= 4000000LL) {
        s_window_start_ms = millis_now();
        s_window_samples = 0;
        s_actual_millihz = 0;
        s_valid = false;
    }
    s_length = length;
    s_hz = hz;
    s_interest_us = now;
    xSemaphoreGive(s_mutex);
}

void brick_live_holdoff(uint32_t milliseconds) {
    if (!s_mutex) return;
    if (xSemaphoreTake(s_mutex, ticks_at_least_one(100)) == pdTRUE) {
        int64_t until=esp_timer_get_time() + ((int64_t)milliseconds * 1000);
        if (until > s_holdoff_until_us) s_holdoff_until_us=until;
        xSemaphoreGive(s_mutex);
    }
}

bool brick_live_copy(brick_live_snapshot_t *out) {
    if (!out || !s_mutex) return false;
    if (xSemaphoreTake(s_mutex, ticks_at_least_one(100)) != pdTRUE) return false;
    bool valid = s_valid && s_latest.length > 0;
    if (valid) memcpy(out, &s_latest, sizeof(*out));
    xSemaphoreGive(s_mutex);
    return valid;
}

void brick_live_get_stats(brick_live_stats_t *out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!s_mutex || xSemaphoreTake(s_mutex, ticks_at_least_one(100)) != pdTRUE) return;
    out->sequence = s_latest.sequence;
    out->samples_ok = s_ok;
    out->samples_failed = s_failed;
    out->age_ms = s_valid ? (uint32_t)(millis_now() - s_latest.sample_ms) : 0;
    out->sample_duration_us = s_latest.sample_duration_us;
    out->actual_millihz = s_actual_millihz;
    out->requested_hz = s_hz;
    out->bytes_per_sample = s_length;
    out->active = s_interest_us > 0 && (esp_timer_get_time() - s_interest_us) < 4000000LL;
    xSemaphoreGive(s_mutex);
}

static void live_task(void *ignored) {
    (void)ignored;
    ts_reply_t *reply = calloc(1, sizeof(*reply));
    uint8_t *block = malloc(BRICK_LIVE_MAX_BYTES);
    if (!reply || !block) { free(reply); free(block); vTaskDelete(NULL); return; }
    for (;;) {
        uint16_t length = 0, hz = 0;
        int64_t interest = 0;
        int64_t holdoff_until = 0;
        if (xSemaphoreTake(s_mutex, ticks_at_least_one(50)) == pdTRUE) {
            length = s_length;
            hz = s_hz;
            interest = s_interest_us;
            holdoff_until = s_holdoff_until_us;
            xSemaphoreGive(s_mutex);
        }
        // No new USB gauge query during the short calibration-priority lease.
        if(esp_timer_get_time() < holdoff_until){
            vTaskDelay(ticks_at_least_one(25));
            continue;
        }
        char signature[160];
        brick_ecu_signature_get(signature, sizeof(signature));
        if (interest <= 0 || (esp_timer_get_time() - interest) >= 4000000LL ||
            !vcp_gateway_usb_ready() || !signature[0] || !length || !hz) {
            if (xSemaphoreTake(s_mutex, ticks_at_least_one(50)) == pdTRUE) {
                s_valid = false;
                xSemaphoreGive(s_mutex);
            }
            vTaskDelay(ticks_at_least_one(100));
            continue;
        }
        // Read every selected output byte in bounded 1024-byte chunks.
        // The ECU's 2248-byte Proteus runtime block needs three USB reads;
        // only a complete, verified result replaces the published snapshot.
        int64_t start_us = esp_timer_get_time();
        esp_err_t err = ESP_OK;
        for (uint16_t offset = 0; offset < length;) {
            uint16_t count = (uint16_t)(length - offset);
            if (count > 1024) count = 1024;
            uint8_t args[4] = {(uint8_t)offset, (uint8_t)(offset >> 8),
                               (uint8_t)count, (uint8_t)(count >> 8)};
            err = ts_request('O', args, sizeof(args), reply, 850);
            if (err != ESP_OK || reply->status != 0 || reply->length != count) {
                if (err == ESP_OK) err = ESP_ERR_INVALID_RESPONSE;
                break;
            }
            memcpy(block + offset, reply->bytes, count);
            offset = (uint16_t)(offset + count);
        }
        uint32_t elapsed_us = (uint32_t)(esp_timer_get_time() - start_us);
        uint32_t now_ms = millis_now();
        if (xSemaphoreTake(s_mutex, ticks_at_least_one(100)) == pdTRUE) {
            if (err == ESP_OK) {
                s_latest.sequence++;
                s_latest.sample_ms = now_ms;
                s_latest.sample_duration_us = elapsed_us;
                s_latest.length = length;
                s_latest.target_hz = hz;
                memcpy(s_latest.bytes, block, length);
                s_valid = true;
                ++s_ok;
                ++s_window_samples;
            } else {
                ++s_failed;
            }
            uint32_t window_ms = now_ms - s_window_start_ms;
            if (window_ms >= 1000) {
                s_actual_millihz = window_ms ? (uint32_t)(((uint64_t)s_window_samples * 1000000U) / window_ms) : 0;
                s_window_start_ms = now_ms;
                s_window_samples = 0;
            }
            xSemaphoreGive(s_mutex);
        }
        // Rate is a target, not an asserted USB speed; slow requests self-limit.
        uint32_t period_ms = 1000U / hz;
        uint32_t spent_ms = elapsed_us / 1000U;
        vTaskDelay(ticks_at_least_one(period_ms > spent_ms ? period_ms - spent_ms : 1));
    }
}

esp_err_t brick_live_init(void) {
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;
    s_window_start_ms = millis_now();
    if (xTaskCreate(live_task, "brick-live-rx", 4096, NULL, 2, NULL) != pdPASS)
        return ESP_ERR_NO_MEM;
    return ESP_OK;
}
