#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Starts the USB CDC/VCP <-> TCP raw byte-stream gateway.
 *
 * The hub's existing Wi-Fi AP is reused. The gateway does not create a
 * second Wi-Fi interface or alter the hub's SSID/password configuration.
 * Returns ESP_OK once the gateway tasks/host stack are started.
 */
int vcp_tcp_gateway_start(void);
bool vcp_gateway_ts_tcp_active(void);
bool vcp_gateway_usb_ready(void);
uint32_t brick_usb_open_attempts(void);
uint32_t brick_usb_open_failures(void);
uint32_t brick_usb_disconnects(void);
uint32_t brick_usb_stack_failures(void);
uint32_t brick_usb_successful_opens(void);
bool brick_usb_stack_started(void);
/* Native BrickOS USB request transport, no TCP connection required. */
#include "esp_err.h"
esp_err_t brick_usb_transaction_begin(void);
esp_err_t brick_usb_transaction_write(const uint8_t *data, size_t len);
size_t brick_usb_transaction_read(uint8_t *dst, size_t len, uint32_t timeout_ms);
void brick_usb_transaction_end(void);

size_t vcp_gateway_ts_diag_log(char *out, size_t out_cap);
bool vcp_gateway_ts_diag_send_log(int fd);
bool vcp_gateway_ts_diag_capture_ready(void);
int32_t vcp_gateway_ts_diag_capture_error(void);
bool vcp_gateway_ts_diag_capture_active(void);
bool vcp_gateway_ts_diag_capture_start(void);
void vcp_gateway_ts_diag_capture_release(void);
bool vcp_tcp_gateway_usb_claimed(void);
void vcp_tcp_gateway_set_usb_owner(bool owner);
uint32_t vcp_gateway_ts_usb_to_tcp_drop_bytes(void);
uint32_t vcp_gateway_ts_tcp_tx_faults(void);
int32_t vcp_gateway_ts_tcp_tx_fault_errno(void);

#ifdef __cplusplus
}
#endif
