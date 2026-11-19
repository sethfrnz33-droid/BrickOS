#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "esp_http_server.h"

void ini_cache_init(void);
bool ini_cache_present(void);
size_t ini_cache_size(void);
esp_err_t ini_cache_upload(httpd_req_t *req);
esp_err_t ini_cache_download(const char *url, const char *ssid, const char *password, char *error, size_t error_size);
esp_err_t ini_cache_serve(httpd_req_t *req);
esp_err_t ini_cache_delete(void);

/* Imports a local file only if its TunerStudio signature matches the live ECU. */
esp_err_t ini_cache_import_exact_file(const char *source_path, const char *ecu_signature);

/* Exact-signature INI output-channel descriptors for a bench-only simulator.
 * A descriptor is never accepted from an unsigned browser-supplied offset. */
typedef struct { unsigned offset; double scale, translate; } brick_bench_u16_t;
bool ini_cache_bench_channels(const char *ecu_signature,
                              brick_bench_u16_t *rpm, brick_bench_u16_t *battery);
