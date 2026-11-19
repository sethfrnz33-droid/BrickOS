#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <math.h>
#include "lwip/sockets.h"
#include "lwip/tcp.h"
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_attr.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "protocol.h"
#include "vcp_tcp_gateway.h"
#include "storage.h"
#include "ini_cache.h"
#include "usb_sd_ini.h"
#include "telemetry.h"
#include "stim_policy.h"

#define MAX_BODY 3500
esp_netif_t *g_ap_netif=NULL;
volatile uint32_t g_http_requests=0;
volatile uint32_t g_http_connections=0;
uint32_t hub_web_http_stack_hwm_words(void){return 0;}
uint32_t hub_web_json_cache_stack_hwm_words(void){return 0;}
// BrickOS v4.2 always includes calibration C/B support.
// A prior sdkconfig must not silently compile the write path out.
static const char *TAG="brick.main";

/* Board diagnostics: both indicators are active HIGH, user-confirmed. */
#define BRICK_LED_HEARTBEAT GPIO_NUM_17
#define BRICK_LED_WIFI_CLIENT GPIO_NUM_18
#define BRICK_BOOT_MAGIC 0xB71C0A23U
RTC_NOINIT_ATTR static uint32_t g_boot_magic;
RTC_NOINIT_ATTR static uint32_t g_boot_counter;
// A query crash leaves its last stage intact across a warm reboot.
#define BRICK_QUERY_MAGIC 0xB71C3301U
RTC_NOINIT_ATTR static uint32_t g_query_magic;
RTC_NOINIT_ATTR static uint32_t g_query_count;
RTC_NOINIT_ATTR static uint32_t g_query_stage;
static const char *brick_query_stage_name(uint32_t stage) {
    switch (stage) {
        case 1: return "HTTP_QUERY_ENTERED";
        case 2: return "USB_PROTOCOL_REQUEST";
        case 3: return "USB_PROTOCOL_RETURNED";
        case 4: return "HTTP_RESPONSE_SENT";
        default: return "NONE";
    }
}
static volatile uint32_t g_wifi_last_disconn_reason = 0;
static volatile uint32_t g_wifi_assoc_count = 0;
static volatile uint32_t g_wifi_disassoc_count = 0;
static volatile uint32_t g_wifi_connected_clients = 0;
static void brick_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    if (base != WIFI_EVENT) return;
    if (id == WIFI_EVENT_AP_STACONNECTED) {
        ++g_wifi_assoc_count;
        ++g_wifi_connected_clients;
        ESP_LOGI(TAG, "WIFI_DIAG: phone associated total=%lu", (unsigned long)g_wifi_assoc_count);
    } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
        ++g_wifi_disassoc_count;
        if (data) g_wifi_last_disconn_reason = ((wifi_event_ap_stadisconnected_t *)data)->reason;
        if (g_wifi_connected_clients) --g_wifi_connected_clients;
        ESP_LOGW(TAG, "WIFI_DIAG: phone disconnected total=%lu reason=%lu uptime_ms=%lld", (unsigned long)g_wifi_disassoc_count, (unsigned long)g_wifi_last_disconn_reason, (long long)(esp_timer_get_time()/1000));
    }
}
static void brick_led_task(void *arg) {
    (void)arg;
    bool heartbeat = false;
    for (;;) {
        heartbeat = !heartbeat;
        gpio_set_level(BRICK_LED_HEARTBEAT, heartbeat ? 1 : 0);
        gpio_set_level(BRICK_LED_WIFI_CLIENT, g_wifi_connected_clients ? 1 : 0);
        vTaskDelay(pdMS_TO_TICKS(250) > 0 ? pdMS_TO_TICKS(250) : 1);
    }
}
static void brick_led_init(void) {
    gpio_config_t leds = {
        .pin_bit_mask = (1ULL << BRICK_LED_HEARTBEAT) | (1ULL << BRICK_LED_WIFI_CLIENT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&leds));
    gpio_set_level(BRICK_LED_HEARTBEAT, 0);
    gpio_set_level(BRICK_LED_WIFI_CLIENT, 0);
    BaseType_t result = xTaskCreatePinnedToCore(brick_led_task, "brick-leds", 2048, NULL, 2, NULL, 1);
    if (result != pdPASS) ESP_LOGE(TAG, "LED diagnostic task creation failed");
}

static int64_t armed_until;
/* Internal trigger stimulation is tracked only after rusEFI acknowledges Start.
 * This is a session-bound command state, NOT a claim about physical outputs. */
static volatile bool g_stim_active = false;
static volatile bool g_stim_watchdog_ready = false;
static char g_stim_signature[160] = "";
static uint32_t g_stim_challenge = 0;
static int64_t g_stim_challenge_until = 0;
static int64_t g_stim_active_until = 0;
static int64_t g_stim_no_clients_since = 0;
#define BRICK_STIM_MAX_US 60000000LL
#define BRICK_STIM_PREP_US 15000000LL

// Calibration write protocol is built into BrickOS v4.2, independent of sdkconfig.
// Actual transmission still requires explicit short-lived arming, a matching
// live ECU signature, an editor page backup, and stopped-engine checks.


#include "web_assets.h"

static esp_err_t json_string(httpd_req_t *r,const char *json,int status){
    httpd_resp_set_type(r,"application/json; charset=utf-8");
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    if(status!=200)httpd_resp_set_status(r,status==400?"400 Bad Request":status==403?"403 Forbidden":status==503?"503 Service Unavailable":"500 Internal Server Error");
    return httpd_resp_sendstr(r,json);
}
static esp_err_t error_reply(httpd_req_t *r,int status,const char *message){
    cJSON *j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"ok",false);cJSON_AddStringToObject(j,"error",message);
    char *s=cJSON_PrintUnformatted(j);esp_err_t e=json_string(r,s?s:"{}",status);
    free(s);cJSON_Delete(j);return e;
}
static esp_err_t asset(httpd_req_t *r, const char *type, const uint8_t *p, size_t len){
    httpd_resp_set_type(r,type);httpd_resp_set_hdr(r,"Cache-Control","no-store");
    return httpd_resp_send(r,(const char *)p,len);
}
static esp_err_t homepage(httpd_req_t *r){return asset(r,"text/html; charset=utf-8",brick_asset_html,brick_asset_html_len);}
static esp_err_t csspage(httpd_req_t *r){return asset(r,"text/css; charset=utf-8",brick_asset_css,brick_asset_css_len);}
static esp_err_t jspage(httpd_req_t *r){return asset(r,"application/javascript; charset=utf-8",brick_asset_js,brick_asset_js_len);}
static esp_err_t corepage(httpd_req_t *r){return asset(r,"application/javascript; charset=utf-8",brick_asset_core,brick_asset_core_len);}
static esp_err_t guidedpage(httpd_req_t *r){return asset(r,"application/javascript; charset=utf-8",brick_asset_guided,brick_asset_guided_len);}
static esp_err_t guideduipage(httpd_req_t *r){return asset(r,"application/javascript; charset=utf-8",brick_asset_guided_ui,brick_asset_guided_ui_len);}
/* Versioned guide text + assets; intentionally static/read-only. DS clients can
 * fetch the SAME catalogue from the ESP later, without shipping new copy. */
static esp_err_t guide_story_js(httpd_req_t *r){return asset(r,"application/javascript; charset=utf-8",brick_asset_guide_story,brick_asset_guide_story_len);}
static esp_err_t guide_story_ui_js(httpd_req_t *r){return asset(r,"application/javascript; charset=utf-8",brick_asset_guide_story_ui,brick_asset_guide_story_ui_len);}
static esp_err_t guide_voice_js(httpd_req_t *r){return asset(r,"application/javascript; charset=utf-8",brick_asset_guide_voice,brick_asset_guide_voice_len);}
static esp_err_t guide_catalog_get(httpd_req_t *r){return asset(r,"application/json; charset=utf-8",brick_asset_guide_catalog,brick_asset_guide_catalog_len);}
static esp_err_t guide_head_get(httpd_req_t *r){return asset(r,"image/png",brick_asset_seth_head,brick_asset_seth_head_len);}
static esp_err_t guide_jaw_get(httpd_req_t *r){return asset(r,"image/png",brick_asset_seth_jaw,brick_asset_seth_jaw_len);}
static esp_err_t guide_mouth_get(httpd_req_t *r){return asset(r,"image/png",brick_asset_seth_mouth,brick_asset_seth_mouth_len);}
static esp_err_t guide_voice_get(httpd_req_t *r){return asset(r,"audio/mpeg",brick_asset_seth_voice,brick_asset_seth_voice_len);}
static cJSON *read_request_json(httpd_req_t *r){
    if(r->content_len<1 || r->content_len>MAX_BODY)return NULL;
    char *s=calloc((size_t)r->content_len+1,1);if(!s)return NULL;
    int got=0;
    while(got<r->content_len){int n=httpd_req_recv(r,s+got,r->content_len-got);if(n<=0){free(s);return NULL;}got+=n;}
    cJSON *obj=cJSON_Parse(s);free(s);return obj;
}
static int nybble(char x){if(x>='0'&&x<='9')return x-'0';if(x>='a'&&x<='f')return x-'a'+10;if(x>='A'&&x<='F')return x-'A'+10;return -1;}
static int unhex(const char *s, uint8_t *dst, size_t cap) {
    if (s == NULL) {
        return 0;
    }
    const size_t n = strlen(s);
    if ((n & 1U) != 0U || n / 2 > cap) {
        return -1;
    }
    for (size_t i = 0; i < n / 2; i++) {
        int a = nybble(s[i * 2]);
        int b = nybble(s[i * 2 + 1]);
        if (a < 0 || b < 0) {
            return -1;
        }
        dst[i] = (uint8_t)((a << 4) | b);
    }
    return (int)(n / 2);
}
static SemaphoreHandle_t g_ecu_sig_lock = NULL;
static char g_ecu_sig[160] = "";
static uint32_t g_ecu_handshake_attempts = 0;
static uint32_t g_ecu_handshake_successes = 0;
static char g_ecu_handshake_result[64] = "WAITING FOR USB";

static void brick_ecu_signature_copy(char *dst, size_t cap) {
    if (!dst || !cap) return;
    dst[0] = 0;
    if (g_ecu_sig_lock && xSemaphoreTake(g_ecu_sig_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        snprintf(dst, cap, "%s", g_ecu_sig);
        xSemaphoreGive(g_ecu_sig_lock);
    }
}
void brick_ecu_signature_get(char *out, size_t cap) {
    brick_ecu_signature_copy(out, cap);
}
static esp_err_t status_get(httpd_req_t *r){
    cJSON *j=cJSON_CreateObject();
    cJSON_AddBoolToObject(j,"ok",true);
    cJSON_AddStringToObject(j,"product","BrickOS");
    cJSON_AddStringToObject(j,"brick_os_build","v4.8");
    cJSON_AddBoolToObject(j,"stim_remote_start_supported",g_stim_watchdog_ready);
    cJSON_AddBoolToObject(j,"stim_remote_stop_supported",true);
    char stim_sig[160];
    brick_ecu_signature_copy(stim_sig, sizeof(stim_sig));
    bool stim_known = g_stim_active && vcp_gateway_usb_ready() &&
                      stim_sig[0] && !strcmp(stim_sig, g_stim_signature);
    cJSON_AddStringToObject(j,"stim_running_state",stim_known?"START_ACKNOWLEDGED":"NOT_STARTED_BY_BRICKOS");
    cJSON_AddNumberToObject(j,"stim_timeout_seconds",stim_known && g_stim_active_until>esp_timer_get_time()?
                            (double)((g_stim_active_until-esp_timer_get_time())/1000000LL):0);
    cJSON_AddStringToObject(j,"ap_ssid","brick");
    cJSON_AddStringToObject(j,"ap_mode","AP_ONLY_OPEN");
    cJSON_AddBoolToObject(j,"internet_download_enabled",false);
    cJSON_AddStringToObject(j,"ap_ip","192.168.4.1");
    cJSON_AddNumberToObject(j,"uptime_ms",(double)(esp_timer_get_time()/1000LL));
    cJSON_AddNumberToObject(j,"reset_reason",esp_reset_reason());
    cJSON_AddNumberToObject(j,"boot_counter",g_boot_counter);
    cJSON_AddNumberToObject(j,"query_count",g_query_count);
    cJSON_AddNumberToObject(j,"last_query_stage_number",g_query_stage);
    cJSON_AddStringToObject(j,"last_query_stage",brick_query_stage_name(g_query_stage));
    cJSON_AddNumberToObject(j,"http_stack_free_words",uxTaskGetStackHighWaterMark(NULL));
    cJSON_AddNumberToObject(j,"wifi_last_disconnect_reason",g_wifi_last_disconn_reason);
    cJSON_AddNumberToObject(j,"wifi_connected_clients",g_wifi_connected_clients);
    cJSON_AddNumberToObject(j,"wifi_assoc_events",g_wifi_assoc_count);
    cJSON_AddNumberToObject(j,"wifi_disconnect_events",g_wifi_disassoc_count);
    cJSON_AddStringToObject(j,"diagnostic_leds","GPIO17 heartbeat (blink), GPIO18 phone joined (solid); active-high");
    cJSON_AddStringToObject(j,"transport","Native rusEFI USB CDC host");
    cJSON_AddStringToObject(j,"usb_gateway","Native USB CDC host");
    cJSON_AddBoolToObject(j,"usb_stack_started",brick_usb_stack_started());
    cJSON_AddNumberToObject(j,"usb_open_attempts",brick_usb_open_attempts());
    cJSON_AddNumberToObject(j,"usb_open_failures",brick_usb_open_failures());
    cJSON_AddNumberToObject(j,"usb_disconnects",brick_usb_disconnects());
    cJSON_AddNumberToObject(j,"usb_stack_failures",brick_usb_stack_failures());
    cJSON_AddNumberToObject(j,"usb_successful_opens",brick_usb_successful_opens());
    cJSON_AddBoolToObject(j,"usb_device_ready",vcp_gateway_usb_ready());
    char session_signature[160];
    brick_ecu_signature_copy(session_signature, sizeof(session_signature));
    cJSON_AddStringToObject(j,"ecu_signature",session_signature);
    cJSON_AddBoolToObject(j,"stim_builtin_profile_available",
                          session_signature[0] && !strcmp(session_signature,
                          "rusEFI master.2026.11.09.proteus_f7.2735508644"));
    cJSON_AddBoolToObject(j,"ecu_session_connected",vcp_gateway_usb_ready() && session_signature[0] != 0);
    cJSON_AddNumberToObject(j,"ecu_handshake_attempts",g_ecu_handshake_attempts);
    cJSON_AddNumberToObject(j,"ecu_handshake_successes",g_ecu_handshake_successes);
    cJSON_AddStringToObject(j,"ecu_handshake_state",g_ecu_handshake_result);
    cJSON_AddStringToObject(j,"usb_device_state",vcp_gateway_usb_ready()?"CONNECTED":"WAITING FOR ECU USB");
    cJSON_AddNumberToObject(j,"last_ok_ms",ts_last_good_ms());
    cJSON_AddNumberToObject(j,"ok_requests",ts_total_ok());
    cJSON_AddNumberToObject(j,"failed_requests",ts_total_errors());
    cJSON_AddStringToObject(j,"last_error",ts_last_error());
    cJSON_AddBoolToObject(j,"write_compiled",true);
    cJSON_AddBoolToObject(j,"write_armed",vcp_gateway_usb_ready() && session_signature[0] && esp_timer_get_time()<armed_until);
    cJSON_AddBoolToObject(j,"write_access_ready",true);
    cJSON_AddBoolToObject(j,"backup_storage_ready",storage_path()!=NULL);
    cJSON_AddStringToObject(j,"backup_storage_status",storage_error());
    cJSON_AddStringToObject(j,"write_mode","ENABLED — AUTO AUTHORIZE DURING APPLY");
    cJSON_AddBoolToObject(j,"logging",log_active());
    cJSON_AddStringToObject(j,"log_name",log_current_name());
    cJSON_AddNumberToObject(j,"samples",log_samples());
    cJSON_AddNumberToObject(j,"log_failures",log_failures());
    cJSON_AddNumberToObject(j,"log_last_sample_ms",log_last_sample_ms());
    cJSON_AddNumberToObject(j,"log_actual_hz",log_effective_hz());
    brick_live_stats_t live;
    brick_live_get_stats(&live);
    cJSON_AddBoolToObject(j,"live_active",live.active);
    cJSON_AddNumberToObject(j,"live_target_hz",live.requested_hz);
    cJSON_AddNumberToObject(j,"live_actual_hz",(double)live.actual_millihz / 1000.0);
    cJSON_AddNumberToObject(j,"live_sample_duration_us",live.sample_duration_us);
    cJSON_AddNumberToObject(j,"live_sample_age_ms",live.age_ms);
    cJSON_AddNumberToObject(j,"live_sequence",live.sequence);
    cJSON_AddNumberToObject(j,"live_sample_bytes",live.bytes_per_sample);
    cJSON_AddNumberToObject(j,"live_samples_ok",live.samples_ok);
    cJSON_AddNumberToObject(j,"live_samples_failed",live.samples_failed);
    bool auto_on=false;uint16_t auto_o=0,auto_l=0,auto_i=0;
    log_get_autostart(&auto_on,&auto_o,&auto_l,&auto_i);
    cJSON_AddBoolToObject(j,"auto_log",auto_on);
    cJSON_AddNumberToObject(j,"auto_log_offset",auto_o);
    cJSON_AddNumberToObject(j,"auto_log_length",auto_l);
    cJSON_AddNumberToObject(j,"auto_log_interval_ms",auto_i);
    cJSON_AddStringToObject(j,"storage",storage_path()?storage_path():"unavailable");
    cJSON_AddStringToObject(j,"storage_error",storage_error());
    cJSON_AddBoolToObject(j,"ini_cached",ini_cache_present());
    cJSON_AddNumberToObject(j,"ini_size",ini_cache_size());
    cJSON_AddBoolToObject(j,"ts_command_inspection_available",true);
    cJSON_AddBoolToObject(j,"ts_physical_output_tests_enabled",false);
    cJSON_AddStringToObject(j,"ecu_sd_ini_state",brick_sd_ini_state());
    cJSON_AddNumberToObject(j,"ecu_sd_ini_attempts",brick_sd_ini_attempts());
    cJSON_AddNumberToObject(j,"ecu_sd_ini_imports",brick_sd_ini_imports());
    cJSON_AddNumberToObject(j,"ecu_sd_ini_candidates",brick_sd_ini_candidates());
    cJSON_AddStringToObject(j,"ecu_sd_ini_last_file",brick_sd_ini_last_file());
    cJSON_AddNumberToObject(j,"ecu_sd_ini_last_error",brick_sd_ini_last_error());
    char *s=cJSON_PrintUnformatted(j);cJSON_Delete(j);
    esp_err_t e=json_string(r,s?s:"{}",200);free(s);return e;
}
/* Compact binary telemetry: 16-byte header + raw output-channel bytes.
 * No JSON, hex expansion or extra USB query on each HTTP refresh.
 * Little-endian header BLV1 / sequence / capture_ms / payload_len / usb_duration_ms.
 */
static void live_put_le16(uint8_t *dst, uint16_t value) {
    dst[0] = (uint8_t)value; dst[1] = (uint8_t)(value >> 8);
}
static void live_put_le32(uint8_t *dst, uint32_t value) {
    dst[0] = (uint8_t)value; dst[1] = (uint8_t)(value >> 8);
    dst[2] = (uint8_t)(value >> 16); dst[3] = (uint8_t)(value >> 24);
}
static esp_err_t live_get(httpd_req_t *r) {
    char query[96] = {0}, lenstr[12] = {0}, hzstr[12] = {0};
    if (httpd_req_get_url_query_str(r, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "length", lenstr, sizeof(lenstr)) != ESP_OK ||
        httpd_query_key_value(query, "hz", hzstr, sizeof(hzstr)) != ESP_OK)
        return error_reply(r, 400, "Supply length=4..3072 and hz=10,20,50,100");
    // Reject malformed input instead of silently accepting partial numeric strings.
    char *end_len = NULL, *end_hz = NULL;
    long bytes = strtol(lenstr, &end_len, 10);
    long hz = strtol(hzstr, &end_hz, 10);
    if (!lenstr[0] || !hzstr[0] || *end_len || *end_hz || bytes < 4 ||
        bytes > BRICK_LIVE_MAX_BYTES ||
        (hz != 10 && hz != 20 && hz != 50 && hz != 100))
        return error_reply(r, 400, "Invalid live capture dimensions");
    brick_live_subscribe((uint16_t)bytes, (uint16_t)hz);
    brick_live_snapshot_t *sample = malloc(sizeof(*sample));
    if (!sample) return error_reply(r, 503, "No telemetry memory");
    if (!brick_live_copy(sample)) {
        free(sample);
        return error_reply(r, 503, "Waiting for first ECU telemetry sample");
    }
    // A previous smaller capture is not a valid reply to a larger request.
    if (sample->length != (uint16_t)bytes ||
        (uint32_t)(esp_timer_get_time()/1000 - sample->sample_ms) > 1200) {
        free(sample);
        return error_reply(r, 503, "Telemetry sample pending or stale");
    }
    uint8_t *packet = malloc(16 + sample->length);
    if (!packet) { free(sample); return error_reply(r, 503, "No response memory"); }
    memcpy(packet, "BLV1", 4);
    live_put_le32(packet + 4, sample->sequence);
    live_put_le32(packet + 8, sample->sample_ms);
    live_put_le16(packet + 12, sample->length);
    live_put_le16(packet + 14, (uint16_t)(sample->sample_duration_us/1000U));
    memcpy(packet + 16, sample->bytes, sample->length);
    size_t size = 16U + sample->length;
    free(sample);
    httpd_resp_set_type(r, "application/octet-stream");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
    esp_err_t result = httpd_resp_send(r, (const char *)packet, size);
    free(packet);
    return result;
}

/* Inspect a ControllerCommands entry from the exact active ECU INI.
 * No command payload is decoded, returned or sent to USB. */
static esp_err_t ts_inspect_get(httpd_req_t *r) {
    char query[128]={0},name[64]={0},signature[160]={0};
    if (httpd_req_get_url_query_str(r,query,sizeof(query))!=ESP_OK ||
        httpd_query_key_value(query,"name",name,sizeof(name))!=ESP_OK)
        return error_reply(r,400,"Supply a named ControllerCommands entry");
    brick_ecu_signature_copy(signature,sizeof(signature));
    if (!vcp_gateway_usb_ready()||!signature[0])
        return error_reply(r,503,"No verified live ECU session");
    brick_ini_command_info_t info={0};
    if (!ini_cache_command_info(signature,name,&info))
        return error_reply(r,503,"Matching ECU INI unavailable or command name invalid");
    cJSON *j=cJSON_CreateObject();
    if (!j)return error_reply(r,503,"No memory for INI inspection");
    cJSON_AddBoolToObject(j,"ok",true);
    cJSON_AddStringToObject(j,"name",name);
    cJSON_AddBoolToObject(j,"declared_in_ini",info.declared);
    cJSON_AddBoolToObject(j,"shown_as_button",info.shown_as_button);
    cJSON_AddStringToObject(j,"label",info.label);
    cJSON_AddStringToObject(j,"mode","INSPECTION_ONLY");
    cJSON_AddBoolToObject(j,"ecu_command_sent",false);
    cJSON_AddBoolToObject(j,"hardware_actuation_supported",false);
    char *result=cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    esp_err_t err=json_string(r,result?result:"{}",200);
    free(result);
    return err;
}
static esp_err_t ts_post(httpd_req_t *r){
    g_query_magic = BRICK_QUERY_MAGIC;
    ++g_query_count;
    g_query_stage = 1;
    cJSON *j=read_request_json(r);if(!j)return error_reply(r,400,"Invalid JSON or request too large");
    const char *cmd=cJSON_GetStringValue(cJSON_GetObjectItem(j,"cmd"));
    const char *args=cJSON_GetStringValue(cJSON_GetObjectItem(j,"args"));
    char op=cmd&&strlen(cmd)==1?cmd[0]:'?';
    bool write=(op=='B'||op=='C');
    if(!strchr("SVOARCBKe",op) || op=='?') {cJSON_Delete(j);return error_reply(r,400,"Command not supported");}
    char active_signature[160];
    brick_ecu_signature_copy(active_signature,sizeof(active_signature));
    const char *requested_signature=cJSON_GetStringValue(cJSON_GetObjectItem(j,"signature"));
    if(write && (!active_signature[0] || !requested_signature ||
                 strcmp(active_signature,requested_signature)!=0 || !vcp_gateway_usb_ready())){
        cJSON_Delete(j);return error_reply(r,403,"Write rejected: ECU session signature is missing or changed.");
    }
    if(write && esp_timer_get_time()>=armed_until){
        cJSON_Delete(j);return error_reply(r,403,"Calibration writes are disarmed. Arm writes from the editor before use.");
    }
    uint8_t *bytes = (uint8_t *)malloc(TS_MAX_DATA);
    if (!bytes) { cJSON_Delete(j); return error_reply(r,500,"Out of memory decoding command"); }
    int n=unhex(args,bytes,TS_MAX_DATA);
    if(n<0){free(bytes);cJSON_Delete(j);return error_reply(r,400,"Malformed hex arguments");}
    if((op=='R'&&n!=6)||(op=='O'&&n!=4&&n!=0)||(op=='B'&&n!=2)||(op=='C'&&n<7)||(op=='K'&&n!=6)||((op=='S'||op=='V'||op=='A'||op=='e')&&n!=0)){
        free(bytes);cJSON_Delete(j);return error_reply(r,400,"Wrong command argument length");
    }
    if(op=='R'){
        uint32_t count=bytes[4]|(bytes[5]<<8);
        if(count==0||count>1024){free(bytes);cJSON_Delete(j);return error_reply(r,400,"Read length must be 1..1024 bytes");}
    }
    if(op=='O'&&n==4){uint32_t count=bytes[2]|(bytes[3]<<8);
        if(count==0||count>1024){free(bytes);cJSON_Delete(j);return error_reply(r,400,"Output-channel read length must be 1..1024 bytes");}}
    if(op=='C'){
        uint32_t count=bytes[4]|(bytes[5]<<8);
        if(count!=(uint32_t)(n-6) ||count>512){free(bytes);cJSON_Delete(j);return error_reply(r,400,"Invalid chunk-write count");}
    }
    uint32_t timeout=2500;
    const cJSON *timer=cJSON_GetObjectItem(j,"timeout_ms");
    if(cJSON_IsNumber(timer)&&timer->valueint>=500&&timer->valueint<=12000)timeout=timer->valueint;
    cJSON_Delete(j);
    ts_reply_t *reply = (ts_reply_t *)calloc(1, sizeof(ts_reply_t));
    if (!reply) { free(bytes); return error_reply(r,500,"Out of memory receiving ECU reply"); }
    reply->status = 0xff;
    // Give calibration reads and writes precedence over the continuous gauge sampler.
    // This avoids 100 Hz read contention during page backup, verify, and burn.
    if(op=='R'||op=='C'||op=='B')brick_live_holdoff(1800);
    g_query_stage = 2;
    esp_err_t e=ts_request(op,bytes,n,reply,timeout);
    g_query_stage = 3;
    free(bytes);
    if(e!=ESP_OK) { free(reply); return error_reply(r,503,ts_last_error()); }
    // Keep a confirmed active write transaction from timing out midway through
    // multi-page editing; it is still explicitly disarmed by the editor or USB loss.
    if(write)armed_until=esp_timer_get_time()+60000000LL;
    // If a successful request has a response, provide unmodified payload bytes as hex.
    char *hex=malloc(reply->length*2+1);if(!hex){free(reply);return error_reply(r,500,"Out of memory");}
    static const char hexd[]="0123456789abcdef";
    for(size_t i=0;i<reply->length;i++){hex[2*i]=hexd[reply->bytes[i]>>4];hex[2*i+1]=hexd[reply->bytes[i]&15];}
    hex[reply->length*2]=0;
    cJSON *response=cJSON_CreateObject();cJSON_AddBoolToObject(response,"ok",true);
    cJSON_AddNumberToObject(response,"status",reply->status);
    cJSON_AddNumberToObject(response,"length",reply->length);
    cJSON_AddStringToObject(response,"hex",hex);
    free(hex);free(reply);char *s=cJSON_PrintUnformatted(response);cJSON_Delete(response);
    e=json_string(r,s?s:"{}",200);free(s);g_query_stage=4;return e;
}
/* Read fresh RPM and VBatt from the ECU, not from cached dashboard state.
 * In addition to a matching cached INI, the exact build shipped with this
 * project has a built-in, fixed channel descriptor verified against the
 * generated rusEFI INI. Unknown firmware cannot use these offsets.
 */
static const char *stim_builtin_signature =
    "rusEFI master.2026.11.09.proteus_f7.2735508644";
static bool stim_get_channels(const char *signature,
                              brick_bench_u16_t *rpm, brick_bench_u16_t *battery) {
    if (ini_cache_bench_channels(signature, rpm, battery)) return true;
    if (signature && !strcmp(signature, stim_builtin_signature)) {
        *rpm = (brick_bench_u16_t){.offset=4, .scale=1.0, .translate=0.0};
        *battery = (brick_bench_u16_t){.offset=36, .scale=0.001, .translate=0.0};
        return true;
    }
    return false;
}
static bool stim_session_match(const char *requested, char *active, size_t capacity) {
    brick_ecu_signature_copy(active, capacity);
    return requested && active[0] && !strcmp(requested, active) && vcp_gateway_usb_ready();
}
static bool stim_read_u16(const brick_bench_u16_t *field, double *reading) {
    if (!field || !reading || field->offset > 65533) return false;
    uint8_t args[4] = {(uint8_t)field->offset,(uint8_t)(field->offset>>8),2,0};
    ts_reply_t *reply = calloc(1,sizeof(*reply));
    if (!reply) return false;
    brick_live_holdoff(1000);
    esp_err_t err=ts_request('O',args,sizeof(args),reply,3500);
    bool ok=err==ESP_OK && reply->status==0 && reply->length==2;
    if (ok) {
        uint16_t raw=(uint16_t)reply->bytes[0]|((uint16_t)reply->bytes[1]<<8);
        *reading=raw*field->scale+field->translate;
        ok=isfinite(*reading);
    }
    free(reply);
    return ok;
}
/* NULL means every requirement passed. This preserves independent fresh
 * RPM and battery checks while reporting the *actual* failed stage.
 * Never substitute a stale gauge reading for a failed USB request. */
static const char *stim_bench_problem(const char *signature, bool require_zero_rpm,
                                      double *rpm_out, double *vbat_out) {
    brick_bench_u16_t rpm={0},battery={0};
    if (!stim_get_channels(signature,&rpm,&battery))
        return "No matching INI or built-in firmware channel profile";
    double speed=0,voltage=0;
    bool speed_fresh=stim_read_u16(&rpm,&speed);
    bool voltage_fresh=stim_read_u16(&battery,&voltage);
    if (!speed_fresh) return "ECU RPM USB read failed; cannot confirm stopped engine";
    if (!voltage_fresh) return "ECU battery-voltage USB read failed";
    if (!isfinite(speed) || speed < -0.1 || speed > 35000.0)
        return "ECU reported invalid RPM";
    if (!isfinite(voltage) || voltage < -0.1)
        return "ECU reported invalid battery voltage";
    if (voltage >= 1.0)
        return "ECU reports battery voltage >= 1 V; bench Start blocked";
    if (require_zero_rpm && fabs(speed)>0.1)
        return "ECU reports nonzero RPM while internal stim is OFF";
    if (!brick_stim_policy(!require_zero_rpm,true,speed,true,voltage))
        return "ECU stim safety policy rejected fresh readings";
    if (rpm_out)*rpm_out=speed;
    if (vbat_out)*vbat_out=voltage;
    return NULL;
}
static bool stim_stop_command(void) {
    if (!vcp_gateway_usb_ready()) return false;
    const uint8_t args[] = {0x00,0x14,0x00,0x0f};
    ts_reply_t *reply = calloc(1,sizeof(*reply));
    if (!reply) return false;
    brick_live_holdoff(1200);
    esp_err_t err=ts_request('Z',args,sizeof(args),reply,4500);
    bool ok=err==ESP_OK && (reply->status==0 || reply->status==4);
    free(reply);
    if (ok) {g_stim_active=false;g_stim_signature[0]=0;g_stim_active_until=0;}
    return ok;
}
/* Preflight is step one. A challenge is bound to a verified ECU session and
 * expires after 15 seconds; it is not issued when engine RPM is nonzero and
 * this bridge has not already acknowledged stimulation for that session. */
static esp_err_t stim_preflight_post(httpd_req_t *r) {
    cJSON *j=read_request_json(r);
    if (!j)return error_reply(r,400,"Preflight requires JSON");
    const char *signature=cJSON_GetStringValue(cJSON_GetObjectItem(j,"signature"));
    char active[160];
    bool session_ok=g_stim_watchdog_ready && stim_session_match(signature,active,sizeof(active));
    cJSON_Delete(j);
    if (!session_ok)return error_reply(r,403,"ECU signature mismatch or USB disconnected");
    bool already_on=g_stim_active && !strcmp(g_stim_signature,active);
    double rpm=0,voltage=0;
    const char *reason=stim_bench_problem(active,!already_on,&rpm,&voltage);
    if (reason) return error_reply(r,403,reason);
    g_stim_challenge=esp_random();
    if (!g_stim_challenge)g_stim_challenge=1;
    g_stim_challenge_until=esp_timer_get_time()+BRICK_STIM_PREP_US;
    char result[240];
    snprintf(result,sizeof(result),"{\"ok\":true,\"token\":\"%08lx\",\"already_active\":%s,\"rpm\":%.1f,\"vbat\":%.3f,\"expires_in_seconds\":15}",
             (unsigned long)g_stim_challenge,already_on?"true":"false",rpm,voltage);
    return json_string(r,result,200);
}
/* Start is impossible with just one tap: requires preflight token, same USB
 * signature, deliberate bench confirmation from 3DS, and fresh ECU readings. */
static esp_err_t stim_start_post(httpd_req_t *r) {
    cJSON *j=read_request_json(r);
    if (!j)return error_reply(r,400,"Start requires JSON");
    const char *signature=cJSON_GetStringValue(cJSON_GetObjectItem(j,"signature"));
    const char *token=cJSON_GetStringValue(cJSON_GetObjectItem(j,"token"));
    const char *confirm=cJSON_GetStringValue(cJSON_GetObjectItem(j,"confirm"));
    char active[160],expected[12];
    snprintf(expected,sizeof(expected),"%08lx",(unsigned long)g_stim_challenge);
    bool allowed=g_stim_watchdog_ready && stim_session_match(signature,active,sizeof(active)) &&
        token && g_stim_challenge && strcmp(token,expected)==0 &&
        esp_timer_get_time()<g_stim_challenge_until &&
        confirm && !strcmp(confirm,"BENCH_NO_12V_LR_CONFIRMED");
    cJSON_Delete(j);
    /* Single-use even on a failed preflight: an uncertain request cannot be replayed. */
    g_stim_challenge=0;g_stim_challenge_until=0;
    if (!allowed)return error_reply(r,403,"Start not authorized: session, token, time or L+R confirmation invalid");
    bool already_on=g_stim_active && !strcmp(g_stim_signature,active);
    double rpm=0,voltage=0;
    const char *reason=stim_bench_problem(active,!already_on,&rpm,&voltage);
    if (reason) return error_reply(r,403,reason);
    if (!already_on) {
        const uint8_t args[]={0x00,0x14,0x00,0x0d}; /* rusEFI TS_X14 enable */
        ts_reply_t *reply=calloc(1,sizeof(*reply));
        if (!reply)return error_reply(r,503,"No ECU reply memory");
        brick_live_holdoff(1200);
        esp_err_t err=ts_request('Z',args,sizeof(args),reply,4500);
        bool ok=err==ESP_OK && (reply->status==0 || reply->status==4);
        free(reply);
        if (!ok)return error_reply(r,503,"ECU did not acknowledge internal stim Start; state unknown");
    }
    g_stim_active=true;
    snprintf(g_stim_signature,sizeof(g_stim_signature),"%s",active);
    g_stim_active_until=esp_timer_get_time()+BRICK_STIM_MAX_US;
    g_stim_no_clients_since=0;
    return json_string(r,already_on?
        "{\"ok\":true,\"already_active\":true,\"command_acknowledged\":true,\"running_state_verified\":false}":
        "{\"ok\":true,\"already_active\":false,\"command_acknowledged\":true,\"running_state_verified\":false}",200);
}
/* Stop never needs a nonce or an RPM measurement. */
static esp_err_t stim_stop_post(httpd_req_t *r){
    (void)r;
    g_stim_challenge=0;g_stim_challenge_until=0;
    if (!stim_stop_command())
        return error_reply(r,503,"ECU trigger Stop was NOT acknowledged; state UNKNOWN");
    return json_string(r,"{\"ok\":true,\"command_acknowledged\":true,\"running_state_verified\":false}",200);
}
static void stim_watchdog_task(void *arg) {
    (void)arg;
    for (;;) {
        int64_t now=esp_timer_get_time();
        if (g_stim_active) {
            char current[160];brick_ecu_signature_copy(current,sizeof(current));
            if (!vcp_gateway_usb_ready() || !current[0] || strcmp(current,g_stim_signature)) {
                /* USB lost: no stop acknowledgement is possible; invalidate state. */
                g_stim_active=false;g_stim_signature[0]=0;g_stim_active_until=0;
            } else {
                if (g_wifi_connected_clients==0) {
                    if (!g_stim_no_clients_since)g_stim_no_clients_since=now;
                } else g_stim_no_clients_since=0;
                if (now>=g_stim_active_until ||
                    (g_stim_no_clients_since && now-g_stim_no_clients_since>5000000LL)) {
                    bool ok=stim_stop_command();
                    ESP_LOGW(TAG,"Automatic stim Stop %s",ok?"ACKNOWLEDGED":"NOT ACKNOWLEDGED");
                    if (!ok)g_stim_active_until=now+1000000LL; /* retry until acknowledged */
                }
            }
        } else g_stim_no_clients_since=0;
        vTaskDelay(pdMS_TO_TICKS(500)>0?pdMS_TO_TICKS(500):1);
    }
}
static esp_err_t arm_post(httpd_req_t *r){
    cJSON *j=read_request_json(r);if(!j)return error_reply(r,400,"Invalid request");
    const char *confirm=cJSON_GetStringValue(cJSON_GetObjectItem(j,"confirm"));
    const char *client_signature=cJSON_GetStringValue(cJSON_GetObjectItem(j,"signature"));
    char active_signature[160];brick_ecu_signature_copy(active_signature,sizeof(active_signature));
    bool signature_ok=client_signature && active_signature[0] && !strcmp(client_signature,active_signature) && vcp_gateway_usb_ready();
    bool yes=confirm&&strcmp(confirm,"BENCH_ONLY_ENGINE_STOPPED_BACKUP_SAVED")==0;
    cJSON_Delete(j);
    if(!yes)return error_reply(r,403,"Bench confirmation required");
    if(!signature_ok)return error_reply(r,403,"Verified live ECU signature is required to arm writes");
    armed_until=esp_timer_get_time()+60000000LL;
    return json_string(r,"{\"ok\":true,\"armed_seconds\":60}",200);
}
static esp_err_t disarm_post(httpd_req_t *r){armed_until=0;return json_string(r,"{\"ok\":true}",200);}
static esp_err_t start_log_post(httpd_req_t *r){
    cJSON *j=read_request_json(r);if(!j)return error_reply(r,400,"Invalid request");
    const cJSON *off=cJSON_GetObjectItem(j,"offset"),*len=cJSON_GetObjectItem(j,"length"),*inter=cJSON_GetObjectItem(j,"interval_ms");
    if(!cJSON_IsNumber(off)||!cJSON_IsNumber(len)||!cJSON_IsNumber(inter)){
        cJSON_Delete(j);return error_reply(r,400,"offset, length and interval_ms are required");
    }
    int o=off->valueint,l=len->valueint,t=inter->valueint;cJSON_Delete(j);
    if(o<0||o>65535||l<1||l>512||t<50||t>5000)return error_reply(r,400,"Invalid log dimensions");
    esp_err_t e=log_begin(o,l,t);
    if(e!=ESP_OK)return error_reply(r,503,storage_error());
    return json_string(r,"{\"ok\":true}",200);
}
static esp_err_t stop_log_post(httpd_req_t *r){log_end();return json_string(r,"{\"ok\":true}",200);}
static esp_err_t autolog_post(httpd_req_t *r){
    cJSON *j=read_request_json(r);if(!j)return error_reply(r,400,"Invalid request");
    const cJSON *enabled=cJSON_GetObjectItem(j,"enabled");
    const cJSON *off=cJSON_GetObjectItem(j,"offset"),*len=cJSON_GetObjectItem(j,"length"),*inter=cJSON_GetObjectItem(j,"interval_ms");
    if(!cJSON_IsBool(enabled)||!cJSON_IsNumber(off)||!cJSON_IsNumber(len)||!cJSON_IsNumber(inter)){
        cJSON_Delete(j);return error_reply(r,400,"enabled, offset, length, interval_ms required");
    }
    int o=off->valueint,l=len->valueint,t=inter->valueint;
    bool on=cJSON_IsTrue(enabled);cJSON_Delete(j);
    if(o<0||o>65535||l<1||l>512||t<50||t>5000||o+l>65535)
        return error_reply(r,400,"Invalid autonomous recorder settings");
    esp_err_t e=log_set_autostart(on,o,l,t);
    if(e!=ESP_OK)return error_reply(r,503,esp_err_to_name(e));
    return json_string(r,"{\"ok\":true}",200);
}

static bool valid_name(const char *x){
    size_t n=x?strlen(x):0;if(n<8||n>=48||strncmp(x,"log_",4)||strcmp(x+n-4,".blg"))return false;
    for(size_t i=4;i<n-4;i++)if((x[i]<'0'||x[i]>'9')&&(x[i]<'a'||x[i]>'f'))return false;
    return true;
}
static bool query_name(httpd_req_t *r,char *name,size_t cap){
    char q[100];if(httpd_req_get_url_query_str(r,q,sizeof(q))!=ESP_OK)return false;
    if(httpd_query_key_value(q,"name",name,cap)!=ESP_OK)return false;
    return valid_name(name);
}
static esp_err_t list_logs_get(httpd_req_t *r){
    const char *root=storage_path();if(!root)return error_reply(r,503,storage_error());
    DIR *d=opendir(root);if(!d)return error_reply(r,503,"Unable to list logs");
    cJSON *j=cJSON_CreateObject(),*list=cJSON_AddArrayToObject(j,"files");
    struct dirent *item;int count=0;
    while((item=readdir(d)) && count<80){
        if (!valid_name(item->d_name)) {
            continue;
        }
        // dirent.d_name may hold up to 255 characters. Explicitly check both
        // strings before constructing a path to avoid truncation and -Werror.
        size_t root_len = strlen(root);
        size_t name_len = strlen(item->d_name);
        char path[108];
        if (root_len + 1 + name_len + 1 > sizeof(path)) {
            continue;
        }
        memcpy(path, root, root_len);
        path[root_len] = '/';
        memcpy(path + root_len + 1, item->d_name, name_len + 1);
        struct stat st;
        if (stat(path, &st) != 0) {
            continue;
        }
        cJSON *o=cJSON_CreateObject();cJSON_AddStringToObject(o,"name",item->d_name);
        cJSON_AddNumberToObject(o,"size",st.st_size);
        cJSON_AddItemToArray(list,o);++count;
    }
    closedir(d);cJSON_AddBoolToObject(j,"ok",true);
    char *s=cJSON_PrintUnformatted(j);cJSON_Delete(j);
    esp_err_t e=json_string(r,s?s:"{}",200);free(s);return e;
}
static esp_err_t download_log_get(httpd_req_t *r){
    char name[48];if(!query_name(r,name,sizeof(name)))return error_reply(r,400,"Invalid filename");
    const char *root=storage_path();if(!root)return error_reply(r,503,storage_error());
    if(log_active()&&strcmp(log_current_name(),name)==0)return error_reply(r,403,"Stop this recording before downloading");
    char path[108];snprintf(path,sizeof(path),"%s/%s",root,name);
    FILE *f=fopen(path,"rb");if(!f)return error_reply(r,400,"File not found");
    httpd_resp_set_type(r,"application/octet-stream");
    char disposition[95];snprintf(disposition,sizeof(disposition),"attachment; filename=\"%s\"",name);
    httpd_resp_set_hdr(r,"Content-Disposition",disposition);
    uint8_t *buf=malloc(8192);if(!buf){fclose(f);return error_reply(r,503,"Not enough memory to stream log");}size_t n;esp_err_t e=ESP_OK;
    while((n=fread(buf,1,8192,f))>0){e=httpd_resp_send_chunk(r,(const char *)buf,n);if(e!=ESP_OK)break;}
    free(buf);fclose(f);if(e!=ESP_OK)return e;return httpd_resp_send_chunk(r,NULL,0);
}
static esp_err_t delete_log(httpd_req_t *r){
    char name[48];if(!query_name(r,name,sizeof(name)))return error_reply(r,400,"Invalid filename");
    const char *root=storage_path();if(!root)return error_reply(r,503,storage_error());
    if(log_active()&&strcmp(log_current_name(),name)==0)return error_reply(r,403,"Stop active recording first");
    char path[108];snprintf(path,sizeof(path),"%s/%s",root,name);
    if(remove(path)!=0)return error_reply(r,400,"Unable to remove file");
    return json_string(r,"{\"ok\":true}",200);
}
/* Preserve an exact full-page ECU image on the ESP32 itself.  This avoids
 * Safari's localStorage/IndexedDB failures and its download popup in the
 * middle of a calibration transaction.  Write it atomically before arming. */
#define BACKUP_MAX_SIZE 65536
static void backup_file_path(char *out,size_t cap,const char *mount,int page,bool temp){
    snprintf(out,cap,"%s/backup_pg%02d.%s",mount,page,temp?"tmp":"bin");
}
static uint32_t crc_step(uint32_t crc,const uint8_t *p,size_t len){
    while(len--){crc^=*p++;for(int i=0;i<8;i++)crc=(crc>>1)^((crc&1)?0xedb88320U:0);}
    return crc;
}
static esp_err_t backup_post(httpd_req_t *r){
    cJSON *j=read_request_json(r);
    if(!j)return error_reply(r,400,"Invalid backup JSON");
    cJSON *pg=cJSON_GetObjectItem(j,"page"),*id=cJSON_GetObjectItem(j,"page_id"),*sz=cJSON_GetObjectItem(j,"size");
    const char *sig=cJSON_GetStringValue(cJSON_GetObjectItem(j,"signature"));
    char expected_signature[160]={0};
    if(sig)snprintf(expected_signature,sizeof(expected_signature),"%s",sig);
    char current[160];brick_ecu_signature_copy(current,sizeof(current));
    bool same=sig&&current[0]&&!strcmp(sig,current)&&vcp_gateway_usb_ready();
    bool valid=cJSON_IsNumber(pg)&&cJSON_IsNumber(id)&&cJSON_IsNumber(sz)&&
      pg->valueint>=1&&pg->valueint<=64&&id->valueint>=0&&id->valueint<=65535&&
      sz->valueint>0&&sz->valueint<=BACKUP_MAX_SIZE;
    int page=valid?pg->valueint:0,page_id=valid?id->valueint:0,size=valid?sz->valueint:0;
    cJSON_Delete(j);
    if(!same)return error_reply(r,403,"Backup requires the matching connected ECU signature");
    if(!valid)return error_reply(r,400,"Invalid backup page, page identifier or size");
    const char *mount=storage_path();if(!mount)return error_reply(r,503,"ESP32 backup storage not mounted");
    char temp[112],final[112];backup_file_path(temp,sizeof(temp),mount,page,true);
    backup_file_path(final,sizeof(final),mount,page,false);
    FILE *out=fopen(temp,"wb");
    if(!out)return error_reply(r,503,"Cannot open temporary ECU backup on ESP32 storage");
    ts_reply_t *reply=calloc(1,sizeof(*reply));
    if(!reply){fclose(out);remove(temp);return error_reply(r,503,"Backup read allocation failed");}
    uint32_t crc=0xffffffffU;
    const char *failure_message=NULL;
    for(int off=0;off<size;){
      int part=size-off;if(part>1024)part=1024;
      uint8_t args[6]={(uint8_t)page_id,(uint8_t)(page_id>>8),(uint8_t)off,(uint8_t)(off>>8),(uint8_t)part,(uint8_t)(part>>8)};
      // USB telemetry is deliberately paused during full calibration reads.
      brick_live_holdoff(2000);
      esp_err_t result=ts_request('R',args,sizeof(args),reply,6000);
      if(result!=ESP_OK||reply->status!=0||reply->length!=(size_t)part){
        failure_message="ECU failed to return a complete backup page";break;
      }
      if(fwrite(reply->bytes,1,(size_t)part,out)!=(size_t)part){
        failure_message="ESP32 backup storage is full or unwritable";break;
      }
      crc=crc_step(crc,reply->bytes,(size_t)part);off+=part;
    }
    free(reply);
    char after_signature[160];brick_ecu_signature_copy(after_signature,sizeof(after_signature));
    if(!vcp_gateway_usb_ready()||strcmp(after_signature,expected_signature))
      failure_message="ECU disconnected or changed during full-page backup";
    bool flushed=fflush(out)==0;
    bool closed=fclose(out)==0;
    if(!flushed||!closed){if(!failure_message)failure_message="ECU backup could not be flushed to storage";}
    if(failure_message){remove(temp);return error_reply(r,503,failure_message);}
    // Verify what is actually on flash/SD before making this backup available.
    FILE *in=fopen(temp,"rb");
    if(!in){remove(temp);return error_reply(r,503,"Backup file cannot be reopened");}
    uint8_t check[512];size_t n;size_t checked=0;uint32_t stored_crc=0xffffffffU;
    while((n=fread(check,1,sizeof(check),in))>0){checked+=n;stored_crc=crc_step(stored_crc,check,n);}
    bool io_ok=!ferror(in);fclose(in);
    crc^=0xffffffffU;stored_crc^=0xffffffffU;
    if(!io_ok||checked!=(size_t)size||crc!=stored_crc){
       remove(temp);return error_reply(r,503,"Backup CRC or length verification failed");
    }
    char previous[112];snprintf(previous,sizeof(previous),"%s/backup_pg%02d.old",mount,page);
    remove(previous);bool had_previous=rename(final,previous)==0;
    if(rename(temp,final)!=0){
       if(had_previous)rename(previous,final);
       remove(temp);return error_reply(r,503,"Could not finalize verified ECU backup");
    }
    if(had_previous)remove(previous);
    cJSON *res=cJSON_CreateObject();cJSON_AddBoolToObject(res,"ok",true);
    cJSON_AddNumberToObject(res,"page",page);cJSON_AddNumberToObject(res,"length",size);
    cJSON_AddNumberToObject(res,"crc32",(double)crc);
    cJSON_AddStringToObject(res,"stored","ESP32_STORAGE");
    char *json=cJSON_PrintUnformatted(res);cJSON_Delete(res);
    esp_err_t e=json_string(r,json?json:"{}",200);free(json);return e;
}
static esp_err_t backup_get(httpd_req_t *r){
    char query[64],p[12];
    if(httpd_req_get_url_query_str(r,query,sizeof(query))!=ESP_OK ||
       httpd_query_key_value(query,"page",p,sizeof(p))!=ESP_OK)
       return error_reply(r,400,"Missing backup page");
    char *end=NULL;long page=strtol(p,&end,10);
    if(*end||page<1||page>64)return error_reply(r,400,"Invalid backup page");
    const char *mount=storage_path();if(!mount)return error_reply(r,503,"Backup storage unavailable");
    char path[112];backup_file_path(path,sizeof(path),mount,(int)page,false);
    FILE *in=fopen(path,"rb");if(!in)return error_reply(r,400,"No saved backup for that page");
    httpd_resp_set_type(r,"application/octet-stream");
    char disposition[96];snprintf(disposition,sizeof(disposition),"attachment; filename=\"brick_ecu_page%02ld_prewrite.bin\"",page);
    httpd_resp_set_hdr(r,"Content-Disposition",disposition);
    httpd_resp_set_hdr(r,"Cache-Control","no-store");
    uint8_t buf[1024];size_t n;esp_err_t e=ESP_OK;
    while((n=fread(buf,1,sizeof(buf),in))>0){e=httpd_resp_send_chunk(r,(const char *)buf,n);if(e!=ESP_OK)break;}
    bool ok=!ferror(in);fclose(in);
    if(e!=ESP_OK)return e;
    if(!ok)return ESP_FAIL;
    return httpd_resp_send_chunk(r,NULL,0);
}
// INI is stored on the ESP32 internal SPIFFS, not in phone/browser storage.
static esp_err_t ini_status_get(httpd_req_t *r){
    cJSON *j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"ok",true);
    cJSON_AddBoolToObject(j,"cached",ini_cache_present());
    cJSON_AddNumberToObject(j,"size",ini_cache_size());
    char *s=cJSON_PrintUnformatted(j);cJSON_Delete(j);
    esp_err_t e=json_string(r,s?s:"{}",200);free(s);return e;
}
static esp_err_t ini_upload_post(httpd_req_t *r){
    esp_err_t e=ini_cache_upload(r);
    if(e!=ESP_OK)return error_reply(r,400,esp_err_to_name(e));
    return json_string(r,"{\"ok\":true}",200);
}
static esp_err_t ini_remove_delete(httpd_req_t *r){
    esp_err_t e=ini_cache_delete();
    if(e!=ESP_OK)return error_reply(r,503,esp_err_to_name(e));
    return json_string(r,"{\"ok\":true}",200);
}
// AP ONLY: no STA network interface, no internet INI download and no APSTA transitions.
#define BRICK_AP_NAME "brick"
static void wifi_start(void){
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, brick_wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, brick_wifi_event, NULL));
    esp_netif_t *ap_netif=esp_netif_create_default_wifi_ap();
    g_ap_netif=ap_netif;
    ESP_ERROR_CHECK(ap_netif ? ESP_OK : ESP_ERR_NO_MEM);
    wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t ap={0};
    memcpy(ap.ap.ssid,BRICK_AP_NAME,sizeof(BRICK_AP_NAME)-1);
    ap.ap.ssid_len=sizeof(BRICK_AP_NAME)-1;
    ap.ap.password[0]=0;
    ap.ap.channel=6;
    ap.ap.max_connection=4;
    ap.ap.authmode=WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&ap));
    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_mode_t mode;
    ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));
    if (mode!=WIFI_MODE_AP) { ESP_LOGE(TAG,"Unexpected Wi-Fi mode %d",mode); abort(); }
    ESP_LOGI(TAG,"AP READY: SSID=brick, OPEN (no password), http://192.168.4.1/");
}
static void http_start(void){
    httpd_config_t cfg=HTTPD_DEFAULT_CONFIG();cfg.stack_size=16384;cfg.max_uri_handlers=42;
    cfg.lru_purge_enable=true; cfg.recv_wait_timeout=8;cfg.send_wait_timeout=15;
    httpd_handle_t server=NULL;ESP_ERROR_CHECK(httpd_start(&server,&cfg));
    const httpd_uri_t routes[]={
      {.uri="/",.method=HTTP_GET,.handler=homepage},
      {.uri="/app.css",.method=HTTP_GET,.handler=csspage},
      {.uri="/app.js",.method=HTTP_GET,.handler=jspage},
      {.uri="/core.mjs",.method=HTTP_GET,.handler=corepage},
      {.uri="/guided.mjs",.method=HTTP_GET,.handler=guidedpage},
      {.uri="/guided-ui.mjs",.method=HTTP_GET,.handler=guideduipage},
      {.uri="/guide-story.mjs",.method=HTTP_GET,.handler=guide_story_js},
      {.uri="/guide-story-ui.mjs",.method=HTTP_GET,.handler=guide_story_ui_js},
      {.uri="/guide-voice.mjs",.method=HTTP_GET,.handler=guide_voice_js},
      {.uri="/api/guide/v1/catalog",.method=HTTP_GET,.handler=guide_catalog_get},
      {.uri="/guide/seth-head.png",.method=HTTP_GET,.handler=guide_head_get},
      {.uri="/guide/seth-jaw.png",.method=HTTP_GET,.handler=guide_jaw_get},
      {.uri="/guide/seth-mouth.png",.method=HTTP_GET,.handler=guide_mouth_get},
      {.uri="/guide/voice.mp3",.method=HTTP_GET,.handler=guide_voice_get},
      {.uri="/api/status",.method=HTTP_GET,.handler=status_get},
      {.uri="/api/ts",.method=HTTP_POST,.handler=ts_post},
      {.uri="/api/ts/inspect",.method=HTTP_GET,.handler=ts_inspect_get},
      {.uri="/api/live",.method=HTTP_GET,.handler=live_get},
      {.uri="/api/stim/stop",.method=HTTP_POST,.handler=stim_stop_post},
      {.uri="/api/stim/preflight",.method=HTTP_POST,.handler=stim_preflight_post},
      {.uri="/api/stim/start",.method=HTTP_POST,.handler=stim_start_post},
      {.uri="/api/arm",.method=HTTP_POST,.handler=arm_post},
      {.uri="/api/backup",.method=HTTP_POST,.handler=backup_post},
      {.uri="/api/backup/get",.method=HTTP_GET,.handler=backup_get},
      {.uri="/api/disarm",.method=HTTP_POST,.handler=disarm_post},
      {.uri="/api/log/start",.method=HTTP_POST,.handler=start_log_post},
      {.uri="/api/log/stop",.method=HTTP_POST,.handler=stop_log_post},
      {.uri="/api/log/auto",.method=HTTP_POST,.handler=autolog_post},
      {.uri="/api/logs",.method=HTTP_GET,.handler=list_logs_get},
      {.uri="/api/log/get",.method=HTTP_GET,.handler=download_log_get},
      {.uri="/api/log/delete",.method=HTTP_DELETE,.handler=delete_log},
      {.uri="/api/ini/status",.method=HTTP_GET,.handler=ini_status_get},
      {.uri="/api/ini/file",.method=HTTP_GET,.handler=ini_cache_serve},
      {.uri="/api/ini/upload",.method=HTTP_POST,.handler=ini_upload_post},
      {.uri="/api/ini/file",.method=HTTP_DELETE,.handler=ini_remove_delete},
    };
    for(size_t i=0;i<sizeof(routes)/sizeof(routes[0]);i++)ESP_ERROR_CHECK(httpd_register_uri_handler(server,&routes[i]));
}
/* BrickOS is itself the ECU client. A background task owns initial USB
   signature discovery, independent of a browser request or a TCP connection.
   No transport work is done while the CDC device is absent. */
static void brick_ecu_handshake_task(void *arg) {
    (void)arg;
    bool was_ready = false;
    while (true) {
        bool ready = vcp_gateway_usb_ready();
        if (!ready) {
            if (was_ready) {
                if (xSemaphoreTake(g_ecu_sig_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
                    g_ecu_sig[0] = 0;
                    snprintf(g_ecu_handshake_result, sizeof(g_ecu_handshake_result), "USB DISCONNECTED");
                    xSemaphoreGive(g_ecu_sig_lock);
                }
            }
            armed_until=0; /* A USB disconnect always revokes write authorization. */
            was_ready = false;
            vTaskDelay(pdMS_TO_TICKS(1000) > 0 ? pdMS_TO_TICKS(1000) : 1);
            continue;
        }
        was_ready = true;
        char current[sizeof(g_ecu_sig)];
        brick_ecu_signature_copy(current, sizeof(current));
        if (current[0]) {
            vTaskDelay(pdMS_TO_TICKS(1000) > 0 ? pdMS_TO_TICKS(1000) : 1);
            continue;
        }
        ++g_ecu_handshake_attempts;
        ts_reply_t rep = {.status = 0xff};
        esp_err_t err = ts_request('S', NULL, 0, &rep, 2000);
        if (err == ESP_OK && rep.length >= 7 && rep.length < sizeof(g_ecu_sig) &&
            memcmp(rep.bytes, "rusEFI ", 7) == 0 && vcp_gateway_usb_ready()) {
            size_t n = rep.length;
            while (n && (rep.bytes[n-1] == 0 || rep.bytes[n-1] == '\r' || rep.bytes[n-1] == '\n')) --n;
            if (xSemaphoreTake(g_ecu_sig_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
                memcpy(g_ecu_sig, rep.bytes, n);
                g_ecu_sig[n] = 0;
                ++g_ecu_handshake_successes;
                snprintf(g_ecu_handshake_result, sizeof(g_ecu_handshake_result), "SIGNATURE VERIFIED");
                xSemaphoreGive(g_ecu_sig_lock);
                ESP_LOGI(TAG, "ECU signature verified over USB: %s", g_ecu_sig);
            }
        } else {
            if (xSemaphoreTake(g_ecu_sig_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
                snprintf(g_ecu_handshake_result, sizeof(g_ecu_handshake_result),
                         "SIGNATURE: %s", esp_err_to_name(err == ESP_OK ? ESP_ERR_INVALID_RESPONSE : err));
                xSemaphoreGive(g_ecu_sig_lock);
            }
            ESP_LOGW(TAG, "Native USB ECU signature attempt %lu: %s; will retry",
                     (unsigned long)g_ecu_handshake_attempts, esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(1000) > 0 ? pdMS_TO_TICKS(1000) : 1);
    }
}

static void brick_usb_late_start(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(1000) > 0 ? pdMS_TO_TICKS(1000) : 1);
    ESP_LOGI(TAG,"BOOT_DIAG: starting USB host after 1 second");
    esp_err_t e=(esp_err_t)vcp_tcp_gateway_start();
    if(e!=ESP_OK)ESP_LOGE(TAG,"USB CDC host start failed: %s",esp_err_to_name(e));
    vTaskDelete(NULL);
}

void app_main(void){
    esp_err_t e=nvs_flash_init();
    if(e==ESP_ERR_NVS_NO_FREE_PAGES||e==ESP_ERR_NVS_NEW_VERSION_FOUND){ESP_ERROR_CHECK(nvs_flash_erase());e=nvs_flash_init();}
    ESP_ERROR_CHECK(e);

    // Bring up the browser AP even if STM32 USB is disconnected.
    brick_led_init();
    if (g_boot_magic != BRICK_BOOT_MAGIC || g_boot_counter > 100000000U) {
        g_boot_magic = BRICK_BOOT_MAGIC;
        g_boot_counter = 0;
    }
    ++g_boot_counter;
    if (g_query_magic != BRICK_QUERY_MAGIC || g_query_count > 100000000U) {
        g_query_magic = BRICK_QUERY_MAGIC;
        g_query_count = 0;
        g_query_stage = 0;
    }
    ESP_LOGW(TAG,"BOOT_DIAG: boot=%lu reset_reason=%d, GPIO17 heartbeat HIGH-on, GPIO18 client HIGH-on",(unsigned long)g_boot_counter,esp_reset_reason());
    wifi_start();
    http_start();
    storage_init();
    ini_cache_init();
    e=ts_init();
    if(e!=ESP_OK)ESP_LOGE(TAG,"USB client init failed: %s",esp_err_to_name(e));
    // Nonfatal: a telemetry allocation failure must never disable AP, CDC or SD import.
    e=brick_live_init();
    if(e!=ESP_OK)ESP_LOGE(TAG,"Fast live sampler unavailable: %s",esp_err_to_name(e));
    g_ecu_sig_lock = xSemaphoreCreateMutex();
    if (!g_ecu_sig_lock || xTaskCreate(brick_ecu_handshake_task, "brick-ecu-session", 5120, NULL, 2, NULL) != pdPASS)
        ESP_LOGE(TAG, "ECU handshake retry task creation failed");
    // Give the AP time to become usable before USB host discovery begins.
    // This helps isolate a failing USB stack from an AP/phone association issue.
    BaseType_t usb_task = xTaskCreate(brick_usb_late_start, "brick-usb-start", 4096, NULL, 2, NULL);
    if (usb_task != pdPASS) ESP_LOGE(TAG,"USB startup task creation failed");
    xTaskCreate(log_task,"brick-logger",8192,NULL,4,NULL);
    if (xTaskCreate(stim_watchdog_task,"brick-stim-watchdog",4096,NULL,3,NULL)==pdPASS)
        g_stim_watchdog_ready=true;
    else ESP_LOGE(TAG,"Stimulator watchdog unavailable: START MUST REMAIN DISABLED");
    ESP_LOGI(TAG,"BrickOS USB ONLY: http://192.168.4.1/ on brick (OPEN AP)");
    ESP_LOGI(TAG,"BrickOS owns rusEFI USB CDC directly; no TCP bridge");
}
