#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
esp_err_t brick_sd_ini_host_install(void);
const char *brick_sd_ini_state(void);
unsigned brick_sd_ini_attempts(void);
unsigned brick_sd_ini_imports(void);
unsigned brick_sd_ini_candidates(void);
int brick_sd_ini_last_error(void);
const char *brick_sd_ini_last_file(void);
void brick_ecu_signature_get(char *out, size_t cap);
#ifdef __cplusplus
}
#endif
