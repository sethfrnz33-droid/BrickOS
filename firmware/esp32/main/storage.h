#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
void storage_init(void);
const char *storage_path(void);
const char *storage_error(void);
esp_err_t log_begin(uint16_t offset,uint16_t length,uint16_t interval_ms);
void log_end(void);
bool log_active(void);
uint32_t log_samples(void);
const char *log_current_name(void);
void log_task(void *arg);
// Persistent autonomous recorder, stored in NVS.
esp_err_t log_set_autostart(bool enabled,uint16_t offset,uint16_t length,uint16_t interval_ms);
void log_get_autostart(bool *enabled,uint16_t *offset,uint16_t *length,uint16_t *interval_ms);
uint32_t log_failures(void);
uint32_t log_last_sample_ms(void);
float log_effective_hz(void);
