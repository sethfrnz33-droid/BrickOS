#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "esp_err.h"
#include "driver/twai.h"
#define ESP_ERROR_CHECK(expr) do{esp_err_t e=(expr);(void)e;}while(0)
#define ESP_ERR_NVS_NO_FREE_PAGES 0x110
#define ESP_ERR_NVS_NEW_VERSION_FOUND 0x111
#define GPIO_MODE_INPUT 0
#define GPIO_PULLUP_ONLY 1
#define WIFI_STORAGE_RAM 0
#define WIFI_MODE_AP 2
#define WIFI_MODE_APSTA 3
#define WIFI_AUTH_OPEN 0
#define WIFI_IF_AP 1
#define HTTP_GET 0
#define HTTP_POST 1
#define HTTP_DELETE 2
#define HTTPD_DEFAULT_CONFIG() ((httpd_config_t){0})
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
typedef int wifi_mode_t;
typedef struct {int unused;} wifi_init_config_t;
typedef struct {uint8_t ssid[32],password[64],ssid_len,channel,authmode,max_connection;} wifi_config_ap_t;
typedef struct {wifi_config_ap_t ap;} wifi_config_t;
typedef struct {int unused;} esp_netif_t;
typedef struct {int content_len;} httpd_req_t;
typedef struct {int stack_size,max_uri_handlers,recv_wait_timeout,send_wait_timeout;bool lru_purge_enable;} httpd_config_t;
typedef void *httpd_handle_t;
typedef struct {const char *uri;int method;esp_err_t(*handler)(httpd_req_t*);} httpd_uri_t;
typedef struct cJSON {int valueint;} cJSON;
// Stub declarations: no functions are linked by syntax-only compilation.
int esp_netif_init(void);int esp_event_loop_create_default(void);
esp_netif_t* esp_netif_create_default_wifi_ap(void);
esp_netif_t* esp_netif_create_default_wifi_sta(void);
int esp_wifi_init(const wifi_init_config_t*);int esp_wifi_set_storage(int);
int esp_wifi_set_mode(int);int esp_wifi_set_config(int,const wifi_config_t*);
int esp_wifi_start(void);int esp_wifi_get_mode(wifi_mode_t*);
int nvs_flash_init(void);int nvs_flash_erase(void);
int gpio_get_level(gpio_num_t);int gpio_reset_pin(gpio_num_t);
int gpio_set_direction(gpio_num_t,int);int gpio_set_pull_mode(gpio_num_t,int);
int httpd_resp_set_type(httpd_req_t*,const char*);
int httpd_resp_set_hdr(httpd_req_t*,const char*,const char*);
int httpd_resp_set_status(httpd_req_t*,const char*);
int httpd_resp_sendstr(httpd_req_t*,const char*);
int httpd_resp_send(httpd_req_t*,const char*,int);
int httpd_resp_send_chunk(httpd_req_t*,const char*,size_t);
int httpd_req_recv(httpd_req_t*,char*,size_t);
int httpd_req_get_url_query_str(httpd_req_t*,char*,size_t);
int httpd_query_key_value(const char*,const char*,char*,size_t);
int httpd_start(httpd_handle_t*,const httpd_config_t*);
int httpd_register_uri_handler(httpd_handle_t,const httpd_uri_t*);
cJSON* cJSON_CreateObject(void);cJSON* cJSON_Parse(const char*);
cJSON* cJSON_GetObjectItem(const cJSON*,const char*);
const char *cJSON_GetStringValue(const cJSON*);
cJSON* cJSON_AddArrayToObject(cJSON*,const char*);
void cJSON_AddBoolToObject(cJSON*,const char*,bool);
void cJSON_AddStringToObject(cJSON*,const char*,const char*);
void cJSON_AddNumberToObject(cJSON*,const char*,double);
void cJSON_AddItemToArray(cJSON*,cJSON*);
char* cJSON_PrintUnformatted(const cJSON*);
void cJSON_Delete(cJSON*);
int cJSON_IsNumber(const cJSON*);int cJSON_IsBool(const cJSON*);int cJSON_IsTrue(const cJSON*);
typedef int BaseType_t;
#define pdPASS 1
int xTaskCreate(void(*)(void*),const char*,unsigned int,void*,unsigned int,void*);
int xTaskCreatePinnedToCore(void(*)(void*),const char*,unsigned int,void*,unsigned int,void*,int);
void vTaskDelete(void*);void vTaskDelay(uint32_t);
// Additional storage-only API types.
typedef unsigned int TickType_t;
#define portMAX_DELAY ((unsigned int)-1)
#define NVS_READONLY 0
#define NVS_READWRITE 1
typedef int nvs_handle_t;
int nvs_open(const char*,int,nvs_handle_t*);
int nvs_get_u8(nvs_handle_t,const char*,uint8_t*);
int nvs_get_u16(nvs_handle_t,const char*,uint16_t*);
int nvs_set_u8(nvs_handle_t,const char*,uint8_t);
int nvs_set_u16(nvs_handle_t,const char*,uint16_t);
int nvs_commit(nvs_handle_t);
void nvs_close(nvs_handle_t);
uint32_t esp_random(void);
typedef struct {const char *base_path,*partition_label;int max_files;bool format_if_mount_failed;} esp_vfs_spiffs_conf_t;
int esp_vfs_spiffs_register(const esp_vfs_spiffs_conf_t*);
TickType_t xTaskGetTickCount(void);
void vTaskDelayUntil(TickType_t*,TickType_t);

// v2.2 Wi-Fi and active-high LED diagnostics (host-only declarations).
#define GPIO_NUM_8 8
#define GPIO_NUM_17 17
#define GPIO_NUM_18 18
#define GPIO_MODE_OUTPUT 2
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE 0
#define WIFI_EVENT ((esp_event_base_t)"WIFI_EVENT")
#define WIFI_EVENT_AP_STACONNECTED 10
#define WIFI_EVENT_AP_STADISCONNECTED 11
typedef const char *esp_event_base_t;
typedef struct {unsigned long long pin_bit_mask; int mode,pull_up_en,pull_down_en,intr_type;} gpio_config_t;
int gpio_config(const gpio_config_t*);
int gpio_set_level(gpio_num_t,int);
int esp_event_handler_register(esp_event_base_t,int32_t,void (*)(void *, esp_event_base_t,int32_t,void *),void*);
int esp_reset_reason(void);
typedef struct {unsigned int reason;} wifi_event_ap_stadisconnected_t;
// v2.7 INI cache host stubs
#define HTTPD_500_INTERNAL_SERVER_ERROR 500
#define HTTPD_404_NOT_FOUND 404
int esp_spiffs_mounted(const char*);
int httpd_resp_send_err(httpd_req_t*,int,const char*);
