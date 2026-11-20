/* ESP-hosted, education-only Seth views. No ECU command or write interface. */
#pragma once
#include "esp_http_server.h"

void seth_http_init(void);
esp_err_t seth_http_view(httpd_req_t *req);
esp_err_t seth_http_action(httpd_req_t *req);
