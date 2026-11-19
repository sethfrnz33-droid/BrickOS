#pragma once
#include <stdint.h>
#include "esp_err.h"
typedef int gpio_num_t;
#define TWAI_MODE_NORMAL 0
#define TWAI_STATE_STOPPED 0
#define TWAI_STATE_RUNNING 1
#define TWAI_STATE_BUS_OFF 2
#define TWAI_STATE_RECOVERING 3
typedef struct {int mode;gpio_num_t tx_io;gpio_num_t rx_io;int rx_queue_len;int tx_queue_len;uint32_t alerts_enabled;} twai_general_config_t;
typedef struct {int rate;} twai_timing_config_t;
typedef struct {uint32_t acceptance_code, acceptance_mask;int single_filter;} twai_filter_config_t;
typedef struct {uint32_t identifier;uint8_t extd,rtr,data_length_code;uint8_t data[8];} twai_message_t;
typedef struct {int state;uint32_t bus_error_count,rx_missed_count,tx_error_counter,rx_error_counter;} twai_status_info_t;
#define TWAI_GENERAL_CONFIG_DEFAULT(tx,rx,m) ((twai_general_config_t){.mode=m,.tx_io=tx,.rx_io=rx})
#define TWAI_TIMING_CONFIG_250KBITS() ((twai_timing_config_t){.rate=250000})
#define TWAI_TIMING_CONFIG_500KBITS() ((twai_timing_config_t){.rate=500000})
#define TWAI_TIMING_CONFIG_1MBITS() ((twai_timing_config_t){.rate=1000000})
#define TWAI_FILTER_CONFIG_ACCEPT_ALL() ((twai_filter_config_t){.acceptance_code=0,.acceptance_mask=0xffffffffU,.single_filter=1})
esp_err_t twai_driver_install(const twai_general_config_t *general,const twai_timing_config_t *timing,const twai_filter_config_t *filter);
esp_err_t twai_start(void);
esp_err_t twai_stop(void);
esp_err_t twai_driver_uninstall(void);
esp_err_t twai_transmit(const twai_message_t *message,uint32_t ticks);
esp_err_t twai_receive(twai_message_t *message,uint32_t ticks);
esp_err_t twai_clear_receive_queue(void);
esp_err_t twai_get_status_info(twai_status_info_t *info);
esp_err_t twai_initiate_recovery(void);

#define TWAI_ALERT_TX_SUCCESS 0x00000080u
#define TWAI_ALERT_BUS_OFF 0x00000001u
#define TWAI_ALERT_TX_FAILED 0x00000002u
#define TWAI_ALERT_BUS_ERROR 0x00000004u
#define TWAI_ALERT_RX_QUEUE_FULL 0x00000008u
#define TWAI_ALERT_ERR_PASS 0x00000010u
#define TWAI_ALERT_ARB_LOST 0x00000020u
#define TWAI_ALERT_BUS_RECOVERED 0x00000040u
esp_err_t twai_read_alerts(uint32_t *alerts,uint32_t ticks);
