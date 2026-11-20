#pragma once
#include <stdint.h>
#include <esp_netif.h>
#ifdef __cplusplus
extern "C" {
#endif
extern esp_netif_t *g_ap_netif;
extern volatile uint32_t g_http_requests;
extern volatile uint32_t g_http_connections;
uint32_t hub_web_http_stack_hwm_words(void);
uint32_t hub_web_json_cache_stack_hwm_words(void);
#ifdef __cplusplus
}
#endif
