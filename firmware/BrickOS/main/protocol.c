/* Native BrickOS client for rusEFI TunerStudio CRC-framed protocol over
   the STM32 USB CDC host interface. No CAN, no TCP gateway, no host PC TS. */
#include "protocol.h"
#include "vcp_tcp_gateway.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
static SemaphoreHandle_t s_lock;
static char s_error[128]="USB not queried";
static uint32_t s_ok,s_fail,s_last;
uint32_t ts_crc32(const uint8_t *p,size_t n){uint32_t c=0xffffffffu;for(size_t i=0;i<n;i++){c^=p[i];for(int j=0;j<8;j++)c=(c>>1)^((c&1)?0xedb88320u:0);}return c^0xffffffffu;}
static uint32_t read_be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void write_be32(uint8_t *p,uint32_t x){p[0]=x>>24;p[1]=x>>16;p[2]=x>>8;p[3]=x;}
static uint32_t now_ms(void){return (uint32_t)(esp_timer_get_time()/1000);}
static void failure(const char *op,esp_err_t e){++s_fail;snprintf(s_error,sizeof(s_error),"USB %s: %s",op,esp_err_to_name(e));}
const char *ts_last_error(void){return s_error;}
uint32_t ts_last_good_ms(void){return s_last;}
uint32_t ts_total_ok(void){return s_ok;}
uint32_t ts_total_errors(void){return s_fail;}
esp_err_t ts_init(void){s_lock=xSemaphoreCreateMutex();return s_lock?ESP_OK:ESP_ERR_NO_MEM;}
static esp_err_t usb_read_exact(uint8_t *dst, size_t n, uint32_t timeout_ms)
{
    const int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (n) {
        int64_t remaining = deadline - esp_timer_get_time();
        if (remaining <= 0) return ESP_ERR_TIMEOUT;
        size_t received = brick_usb_transaction_read(dst, n,
            (uint32_t)((remaining + 999) / 1000));
        if (!received) {
            if (!vcp_gateway_usb_ready()) return ESP_ERR_NOT_FOUND;
            continue;
        }
        dst += received;
        n -= received;
    }
    return ESP_OK;
}
esp_err_t ts_request(char command,const uint8_t *args,size_t args_len,ts_reply_t *reply,uint32_t timeout_ms){
    if(!reply||args_len>TS_MAX_DATA-1||(!args&&args_len)||!s_lock)return ESP_ERR_INVALID_ARG;
    if(!timeout_ms)timeout_ms=4000;
    if(xSemaphoreTake(s_lock,pdMS_TO_TICKS(timeout_ms+1000))!=pdTRUE){failure("busy",ESP_ERR_TIMEOUT);return ESP_ERR_TIMEOUT;}
    reply->status=0xff;reply->length=0;
    /* The ESP32 itself is the TS client: wait for USB CDC enumeration,
       then claim a dedicated USB transaction with no loopback TCP socket. */
    TickType_t delay_ticks = pdMS_TO_TICKS(100);
    if (delay_ticks == 0) delay_ticks = 1;
    for (int i = 0; i < 60 && !vcp_gateway_usb_ready(); ++i)
        vTaskDelay(delay_ticks);
    if (!vcp_gateway_usb_ready()) {
        failure("ECU USB device not enumerated", ESP_ERR_NOT_FOUND);
        xSemaphoreGive(s_lock);
        return ESP_ERR_NOT_FOUND;
    }
    esp_err_t e = brick_usb_transaction_begin();
    if (e != ESP_OK) {
        failure("claim native USB", e);
        xSemaphoreGive(s_lock);
        return e;
    }
    // Allocate USB transport frames on the heap: this call is also made from
    // the HTTP server task and must not consume several KB of its stack.
    uint8_t *req = (uint8_t *)malloc(TS_MAX_DATA + 6);
    uint8_t *resp = (uint8_t *)malloc(TS_MAX_DATA + 5);
    if (!req || !resp) {
        free(req); free(resp);
        brick_usb_transaction_end();
        failure("allocate USB buffers", ESP_ERR_NO_MEM);
        xSemaphoreGive(s_lock);
        return ESP_ERR_NO_MEM;
    }
    size_t body=1+args_len;req[0]=(uint8_t)(body>>8);req[1]=(uint8_t)body;req[2]=(uint8_t)command;
    if (args_len) {
        memcpy(req + 3, args, args_len);
    }
    write_be32(req + 2 + body, ts_crc32(req + 2, body));
    e = brick_usb_transaction_write(req, body + 6);
    uint8_t hdr[2];
    if (e == ESP_OK) e = usb_read_exact(hdr, 2, timeout_ms);
    size_t size = 0;
    if (e == ESP_OK) {
        size = ((size_t)hdr[0] << 8) | hdr[1];
        if (size < 1 || size > TS_MAX_DATA + 1) e = ESP_ERR_INVALID_SIZE;
    }
    if (e == ESP_OK) e = usb_read_exact(resp, size + 4, timeout_ms);
    if (e == ESP_OK && read_be32(resp + size) != ts_crc32(resp, size))
        e = ESP_ERR_INVALID_CRC;
    if (e == ESP_OK) {
        reply->status = resp[0];
        reply->length = size - 1;
        if (size > 1) memcpy(reply->bytes, resp + 1, size - 1);
        if (reply->status != 0 && reply->status != 4) e = ESP_ERR_INVALID_RESPONSE;
    }
    if (command == 'S' || e != ESP_OK) {
        ESP_LOGI("brick.usb", "Native USB TS cmd=%c result=%s status=0x%02x bytes=%u",
            command, esp_err_to_name(e), reply->status, (unsigned)reply->length);
    }
    brick_usb_transaction_end();
    free(req);
    free(resp);
    if(e==ESP_OK){++s_ok;s_last=now_ms();snprintf(s_error,sizeof(s_error),"USB connected");}
    else failure("request",e);
    xSemaphoreGive(s_lock);
    return e;
}
