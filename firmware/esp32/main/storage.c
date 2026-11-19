#include "storage.h"
#include "protocol.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_spiffs.h"
#include "nvs.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static SemaphoreHandle_t g_mutex;
static FILE *g_file;
static bool g_ready, g_active;
static uint16_t g_offset, g_size, g_interval;
static uint32_t g_samples, g_failures, g_last_sample;
static uint32_t g_recent_start;
static float g_effective_hz;
static bool g_auto;
static uint16_t g_auto_offset=0,g_auto_length=256,g_auto_interval=100;
static char g_name[48], g_error[100]="Storage not initialized";
static const char *g_mount =
#if CONFIG_BRICK_SD_SPI
    "/sd";
#else
    "/logs";
#endif

static void u16(FILE *f,uint16_t v){uint8_t b[2]={(uint8_t)v,(uint8_t)(v>>8)};fwrite(b,1,2,f);}
static void u32(FILE *f,uint32_t v){uint8_t b[4]={(uint8_t)v,(uint8_t)(v>>8),(uint8_t)(v>>16),(uint8_t)(v>>24)};fwrite(b,1,4,f);}
const char *storage_path(void) {return g_ready ? g_mount : NULL;}
const char *storage_error(void) {return g_error;}
bool log_active(void) {return g_active;}
uint32_t log_samples(void) {return g_samples;}
uint32_t log_failures(void) {return g_failures;}
uint32_t log_last_sample_ms(void) {return g_last_sample;}
float log_effective_hz(void) {return g_effective_hz;}
void log_get_autostart(bool *enabled,uint16_t *offset,uint16_t *length,uint16_t *interval_ms){
    if(enabled)*enabled=g_auto;
    if(offset)*offset=g_auto_offset;
    if(length)*length=g_auto_length;
    if(interval_ms)*interval_ms=g_auto_interval;
}
esp_err_t log_set_autostart(bool enabled,uint16_t offset,uint16_t length,uint16_t interval_ms){
    if(length<1 || length>512 || (uint32_t)offset+length>65535 || interval_ms<50 || interval_ms>5000)return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;esp_err_t e=nvs_open("brick_logger",NVS_READWRITE,&h);
    if(e!=ESP_OK)return e;
    if((e=nvs_set_u8(h,"enabled",enabled?1:0))==ESP_OK)e=nvs_set_u16(h,"offset",offset);
    if(e==ESP_OK)e=nvs_set_u16(h,"length",length);
    if(e==ESP_OK)e=nvs_set_u16(h,"interval",interval_ms);
    if(e==ESP_OK)e=nvs_commit(h);
    nvs_close(h);
    if(e==ESP_OK){g_auto=enabled;g_auto_offset=offset;g_auto_length=length;g_auto_interval=interval_ms;}
    return e;
}
const char *log_current_name(void) {return g_name;}
void storage_init(void) {
    g_mutex=xSemaphoreCreateMutex();
    nvs_handle_t nvs;
    if(nvs_open("brick_logger",NVS_READONLY,&nvs)==ESP_OK){
        uint8_t auto_on=0;
        nvs_get_u8(nvs,"enabled",&auto_on);
        nvs_get_u16(nvs,"offset",&g_auto_offset);
        nvs_get_u16(nvs,"length",&g_auto_length);
        nvs_get_u16(nvs,"interval",&g_auto_interval);
        nvs_close(nvs);
        g_auto=auto_on && g_auto_length>=1 && g_auto_length<=512 &&
               (uint32_t)g_auto_offset+g_auto_length<=65535 &&
               g_auto_interval>=50 && g_auto_interval<=5000;
    }
    esp_err_t e;
#if CONFIG_BRICK_SD_SPI
    sdmmc_host_t host=SDSPI_HOST_DEFAULT();
    host.slot=SPI3_HOST;
    spi_bus_config_t bus={.mosi_io_num=CONFIG_BRICK_SD_MOSI,.miso_io_num=CONFIG_BRICK_SD_MISO,
         .sclk_io_num=CONFIG_BRICK_SD_CLK,.quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=4096};
    e=spi_bus_initialize(host.slot,&bus,SPI_DMA_CH_AUTO);
    if(e==ESP_OK){
        sdspi_device_config_t slot=SDSPI_DEVICE_CONFIG_DEFAULT();
        slot.gpio_cs=CONFIG_BRICK_SD_CS;slot.host_id=host.slot;
        esp_vfs_fat_sdmmc_mount_config_t mount={.format_if_mount_failed=false,.max_files=5,.allocation_unit_size=16384};
        sdmmc_card_t *card=NULL;
        e=esp_vfs_fat_sdspi_mount(g_mount,&host,&slot,&mount,&card);
    }
#else
    esp_vfs_spiffs_conf_t spiffs={.base_path=g_mount,.partition_label="logs",.max_files=5,.format_if_mount_failed=true};
    e=esp_vfs_spiffs_register(&spiffs);
#endif
    g_ready=(e==ESP_OK);
    snprintf(g_error,sizeof(g_error),"%s",g_ready?"OK":esp_err_to_name(e));
    ESP_LOGI("brick.storage","%s storage at %s: %s",
#if CONFIG_BRICK_SD_SPI
      "SD",
#else
      "SPIFFS",
#endif
      g_mount,g_error);
}
esp_err_t log_begin(uint16_t offset,uint16_t length,uint16_t interval_ms) {
    if(!g_ready)return ESP_ERR_INVALID_STATE;
    if(length<1 || length>512 || (uint32_t)offset+length>65535 || interval_ms<50 || interval_ms>5000)return ESP_ERR_INVALID_ARG;
    if(xSemaphoreTake(g_mutex,pdMS_TO_TICKS(500))!=pdTRUE)return ESP_ERR_TIMEOUT;
    if(g_active) {xSemaphoreGive(g_mutex);return ESP_ERR_INVALID_STATE;}
    snprintf(g_name,sizeof(g_name),"log_%08lx.blg",(unsigned long)esp_random());
    char path[110];snprintf(path,sizeof(path),"%s/%s",g_mount,g_name);
    g_file=fopen(path,"wb");
    if(!g_file){snprintf(g_error,sizeof(g_error),"Unable to open log file");xSemaphoreGive(g_mutex);return ESP_FAIL;}
    // BRLOG1 + offset (u16LE) + payload size (u16LE) + interval (u16LE).
    fwrite("BRLOG1",1,6,g_file);u16(g_file,offset);u16(g_file,length);u16(g_file,interval_ms);
    fflush(g_file);
    g_offset=offset;g_size=length;g_interval=interval_ms;g_samples=0;g_failures=0;g_last_sample=0;g_effective_hz=0;g_recent_start=0;g_active=true;
    snprintf(g_error,sizeof(g_error),"OK");
    xSemaphoreGive(g_mutex);
    return ESP_OK;
}
void log_end(void) {
    if(!g_mutex)return;
    xSemaphoreTake(g_mutex,portMAX_DELAY);
    g_active=false;
    if(g_file){fflush(g_file);fclose(g_file);g_file=NULL;}
    xSemaphoreGive(g_mutex);
}
void log_task(void *arg) {
    (void)arg;
    uint8_t p[4];ts_reply_t reply;
    // Boot-time auto-recording begins without a browser or TunerStudio session.
    if(g_auto && g_ready){
        esp_err_t e=log_begin(g_auto_offset,g_auto_length,g_auto_interval);
        ESP_LOGI("brick.storage","Autonomous recording: %s",esp_err_to_name(e));
    }
    TickType_t next=xTaskGetTickCount();
    uint16_t last_interval=0;
    for(;;){
        if(!g_active){last_interval=0;vTaskDelay(pdMS_TO_TICKS(80));continue;}
        uint16_t off=g_offset,len=g_size,interval=g_interval;
        if(interval!=last_interval){next=xTaskGetTickCount();last_interval=interval;}
        p[0]=off;p[1]=off>>8;p[2]=len;p[3]=len>>8;
        memset(&reply,0,sizeof(reply));
        esp_err_t e=ts_request('O',p,4,&reply,1500);
        if(e==ESP_OK && reply.length==len && reply.status==0){
            xSemaphoreTake(g_mutex,portMAX_DELAY);
            if(g_active && g_file){
                uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
                bool good=true;
                u32(g_file,now);
                if(fwrite(reply.bytes,1,len,g_file)!=len)good=false;
                if(good){
                    ++g_samples;g_last_sample=now;
                    if(g_recent_start==0)g_recent_start=now;
                    else if(g_samples%25==0){
                        uint32_t elapsed=now-g_recent_start;
                        if(elapsed)g_effective_hz=(float)25*1000/elapsed;
                        g_recent_start=now;
                    }
                }
                if(!good || ((g_samples%10)==0 && fflush(g_file)!=0)){
                    snprintf(g_error,sizeof(g_error),"Storage write error - logging stopped");
                    g_active=false;fclose(g_file);g_file=NULL;
                }
                long file_size=g_file?ftell(g_file):0;
#if !CONFIG_BRICK_SD_SPI
                if(g_file && file_size>1100000){
                    snprintf(g_error,sizeof(g_error),"SPIFFS log capacity reached");
                    g_active=false;fclose(g_file);g_file=NULL;
                }
#else
                (void)file_size;
#endif
            }
            xSemaphoreGive(g_mutex);
        }else{++g_failures;}
        // Use elapsed-time scheduling instead of request-time + sleep to avoid
        // unnecessarily slowing the actual logging frequency.
        TickType_t now_tick=xTaskGetTickCount();
        if((int32_t)(now_tick-next)>(int32_t)pdMS_TO_TICKS(interval))next=now_tick;
        vTaskDelayUntil(&next,pdMS_TO_TICKS(interval));
    }
}
