#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#define TS_MAX_DATA 1536
typedef struct { uint8_t status; size_t length; uint8_t bytes[TS_MAX_DATA]; } ts_reply_t;
esp_err_t ts_init(void);
esp_err_t ts_request(char command,const uint8_t *args,size_t args_len,ts_reply_t *reply,uint32_t timeout_ms);
const char *ts_last_error(void);
uint32_t ts_last_good_ms(void);
uint32_t ts_total_ok(void);
uint32_t ts_total_errors(void);
uint32_t ts_crc32(const uint8_t *p,size_t n);
