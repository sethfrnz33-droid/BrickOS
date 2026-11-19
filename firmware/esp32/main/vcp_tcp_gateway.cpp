/*
 * vcp_tcp_gateway.cpp
 *
 * High-throughput USB CDC/VCP <-> TCP raw byte-stream gateway.
 *
 * Low-latency TunerStudio version:
 * - TCP -> USB is direct
 * - USB -> TCP goes directly from the CDC callback to the TCP socket
 * - Higher-priority CDC client task
 * - Larger TCP buffers
 * - TCP_NODELAY retained
 * - No per-packet logging
 * - Raw byte stream; no framing added
 * - Live TunerStudio timing statistics exported for phone UI
 */

#include <stdio.h>
#include <string.h>
#include <atomic>
#include <inttypes.h>
#include <fcntl.h>
#include <errno.h>

#include "hub.h"
#include "vcp_tcp_gateway.h"
#include "usb_sd_ini.h"
#include "tsdiag_parser.h"

#include "usb/cdc_acm_host.h"

#include "esp_log.h"
#include "esp_wifi.h"  // wifi_sta_list_t / esp_wifi_ap_get_sta_list
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_freertos_hooks.h"

#include <sys/ioctl.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/stream_buffer.h"

#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/inet.h"

#include <netinet/tcp.h>

#include "driver/gpio.h"

#include "usb/vcp_ch34x.hpp"
#include "usb/vcp_cp210x.hpp"
#include "usb/vcp_ftdi.hpp"
#include "usb/vcp.hpp"
#include "usb/usb_host.h"

using namespace esp_usb;

extern "C" uint32_t hub_web_http_stack_hwm_words(void);
extern "C" uint32_t hub_web_json_cache_stack_hwm_words(void);


/* ============================================================
   CONFIG
   ============================================================ */

#define GATEWAY_TCP_PORT                 2000
#define GATEWAY_MAX_CLIENTS              1

#define GATEWAY_USB_BUFFER_SIZE          1024
#define GATEWAY_TCP_RX_BUFFER_SIZE      4096

#define GATEWAY_USB_BAUDRATE             115200
#define GATEWAY_VBUS_CONTROL_GPIO       (-1)


#define GATEWAY_USB_HOST_PRIORITY        5
#define GATEWAY_CDC_PRIORITY             13
#define GATEWAY_TCP_SERVER_PRIORITY      11

#define GATEWAY_USB_HOST_CORE             0
#define GATEWAY_TCP_SERVER_CORE           1

#define GATEWAY_DIAG_SAMPLE_PERIOD_MS     100
#define GATEWAY_DIAG_SAMPLE_COUNT         151

static const char *GATEWAY_TAG = "VCP_DIRECT";

/* BrickOS native TunerStudio client. The USB callback supplies a single
   ESP32-owned stream, not a TCP socket. Only one request is active at a time. */
static StreamBufferHandle_t g_brick_usb_rx_stream = nullptr;
static std::atomic<bool> g_brick_usb_transaction(false);
/* USB host lifecycle counters, available over the BrickOS Wi-Fi status API. */
static std::atomic<uint32_t> g_brick_usb_open_attempts(0);
static std::atomic<uint32_t> g_brick_usb_open_failures(0);
static std::atomic<uint32_t> g_brick_usb_disconnects(0);
static std::atomic<uint32_t> g_brick_usb_stack_failures(0);
static std::atomic<uint32_t> g_brick_usb_successful_opens(0);



/* ============================================================
   USB / CDC CONSOLE OWNERSHIP
   ============================================================ */

static std::atomic<bool> g_gateway_usb_owner(false);

static vprintf_like_t g_gateway_saved_vprintf = NULL;

static bool g_gateway_log_suppressed = false;


static int gateway_discard_vprintf(
    const char *fmt,
    va_list ap)
{
    (void)fmt;
    (void)ap;

    return 0;
}


bool vcp_tcp_gateway_usb_claimed(void)
{
    return g_gateway_usb_owner.load(
        std::memory_order_relaxed);
}


void vcp_tcp_gateway_set_usb_owner(
    bool owner)
{
    bool current =
        g_gateway_usb_owner.load(
            std::memory_order_relaxed);

    if (current == owner)
        return;

    if (owner)
    {
        g_gateway_saved_vprintf =
            esp_log_set_vprintf(
                gateway_discard_vprintf);

        g_gateway_log_suppressed = true;

        g_gateway_usb_owner.store(
            true,
            std::memory_order_release);
    }
    else
    {
        g_gateway_usb_owner.store(
            false,
            std::memory_order_release);

        if (g_gateway_log_suppressed)
        {
            (void)esp_log_set_vprintf(
                g_gateway_saved_vprintf);

            g_gateway_saved_vprintf = NULL;
            g_gateway_log_suppressed = false;
        }
    }
}


/* ============================================================
   RTOS OBJECTS
   ============================================================ */

static SemaphoreHandle_t
    g_gateway_device_disconnected_sem = NULL;

static SemaphoreHandle_t
    g_gateway_usb_mutex = NULL;

static SemaphoreHandle_t
    g_gateway_start_mutex = NULL;

static std::atomic<int>
    g_gateway_active_client_socket(-1);

static bool
    g_gateway_started = false;

static std::atomic<bool>
    g_gateway_bridge_active(false);

static bool
    g_gateway_usb_stack_started = false;

static bool
    g_gateway_data_tasks_started = false;


/* ============================================================
   TASK HANDLES
   ============================================================ */

static TaskHandle_t
    g_gateway_usb_host_task_handle = NULL;

static TaskHandle_t
    g_gateway_tcp_server_task_handle = NULL;

static TaskHandle_t
    g_gateway_stats_task_handle = NULL;

static TaskHandle_t
    g_gateway_usb_mgr_task_handle = NULL;



/* ============================================================
   USB DEVICE
   ============================================================ */

struct GatewayUsbDevice
{
    bool is_vcp_device;
    bool need_close;

    union
    {
        CdcAcmDevice *vcp_device;
        cdc_acm_dev_hdl_t cdc_handle;
    };

    GatewayUsbDevice()
        :
        is_vcp_device(false),
        need_close(false),
        cdc_handle(nullptr)
    {
    }

    esp_err_t send_data(
        const uint8_t *data,
        size_t len)
    {
        if (
            !need_close ||
            data == nullptr ||
            len == 0)
        {
            return ESP_FAIL;
        }

        if (
            is_vcp_device &&
            vcp_device != nullptr)
        {
            return vcp_device->tx_blocking(
                const_cast<uint8_t *>(data),
                len,
                100);
        }

        if (
            !is_vcp_device &&
            cdc_handle != nullptr)
        {
            return cdc_acm_host_data_tx_blocking(
                cdc_handle,
                const_cast<uint8_t *>(data),
                len,
                100);
        }

        return ESP_FAIL;
    }
};


static GatewayUsbDevice
    g_gateway_usb_device;


/* ============================================================
   GLOBAL STATS
   ============================================================ */

static std::atomic<bool>
    g_gateway_usb_device_ready(false);

static std::atomic<uint64_t>
    g_gateway_usb_rx_bytes(0);

static std::atomic<uint64_t>
    g_gateway_tcp_rx_bytes(0);

static std::atomic<uint64_t>
    g_gateway_usb_rx_chunks(0);

static std::atomic<uint64_t>
    g_gateway_tcp_rx_chunks(0);

static std::atomic<uint64_t>
    g_gateway_usb_to_tcp_bytes(0);

static std::atomic<uint64_t>
    g_gateway_usb_to_tcp_chunks(0);

static std::atomic<uint64_t>
    g_gateway_usb_to_tcp_drop_bytes(0);

static std::atomic<uint64_t>
    g_gateway_tcp_tx_faults(0);

static std::atomic<int>
    g_gateway_tcp_tx_fault_errno(0);


/* ============================================================
   USB TX STATS
   ============================================================ */

static std::atomic<uint64_t>
    g_gateway_usb_tx_count(0);

static std::atomic<uint64_t>
    g_gateway_usb_tx_bytes(0);

static std::atomic<uint64_t>
    g_gateway_usb_tx_time_us(0);

static std::atomic<uint64_t>
    g_gateway_usb_tx_max_us(0);


/* ============================================================
   TCP TX STATS
   ============================================================ */

static std::atomic<uint64_t>
    g_gateway_tcp_tx_count(0);

static std::atomic<uint64_t>
    g_gateway_tcp_tx_bytes(0);

static std::atomic<uint64_t>
    g_gateway_tcp_tx_time_us(0);

static std::atomic<uint64_t>
    g_gateway_tcp_tx_max_us(0);


/* ============================================================
   TUNERSTUDIO LIVE STATS
   ============================================================ */

static std::atomic<uint64_t>
    g_gateway_ts_requests(0);

static std::atomic<uint64_t>
    g_gateway_ts_request_interval_us(0);

static std::atomic<uint64_t>
    g_gateway_ts_request_last_us(0);

static std::atomic<uint64_t>
    g_gateway_ts_roundtrip_us(0);

static std::atomic<uint64_t>
    g_gateway_ts_roundtrip_max_us(0);

static std::atomic<uint64_t>
    g_gateway_ts_usb_rx_count(0);

static std::atomic<uint64_t>
    g_gateway_ts_usb_rx_interval_us(0);

static std::atomic<uint64_t>
    g_gateway_ts_usb_rx_last_us(0);


/* TCP diagnostics */
static std::atomic<uint64_t> g_gateway_tcp_rx_eagain(0);
static std::atomic<uint64_t> g_gateway_tcp_rx_errors(0);
static std::atomic<uint64_t> g_gateway_tcp_tx_eagain(0);
static std::atomic<uint64_t> g_gateway_tcp_tx_partial(0);
static std::atomic<uint64_t> g_gateway_tcp_recv_time_max_us(0);
static std::atomic<uint64_t> g_gateway_tcp_rx_gap_max_us(0);
static std::atomic<uint64_t> g_gateway_tcp_tx_gap_max_us(0);
static std::atomic<uint64_t> g_gateway_tcp_last_rx_us(0);
static std::atomic<uint64_t> g_gateway_tcp_last_tx_us(0);
static std::atomic<int> g_gateway_tcp_last_socket_error(0);

/* USB diagnostics */
static std::atomic<uint64_t> g_gateway_usb_tx_failures(0);
static std::atomic<int> g_gateway_usb_tx_last_error(0);
static std::atomic<uint64_t> g_gateway_usb_rx_interval_max_us(0);
static std::atomic<uint64_t> g_gateway_usb_to_tcp_queue_delay_us(0);
static std::atomic<uint64_t> g_gateway_usb_to_tcp_queue_delay_max_us(0);
static std::atomic<uint64_t> g_gateway_usb_to_tcp_last_enqueue_us(0);

/* TS application-level diagnostics */
static tsdiag_parser_t g_gateway_ts_parser;
static std::atomic<uint64_t> g_gateway_ts_frames(0);
static std::atomic<uint64_t> g_gateway_ts_legacy(0);
static std::atomic<uint64_t> g_gateway_ts_frame_last_us(0);
static std::atomic<uint64_t> g_gateway_ts_frame_interval_us(0);
static std::atomic<uint64_t> g_gateway_ts_frame_interval_sum_us(0);
static std::atomic<uint64_t> g_gateway_ts_frame_interval_samples(0);
static std::atomic<uint64_t> g_gateway_ts_frame_interval_min_us(0);
static std::atomic<uint64_t> g_gateway_ts_frame_interval_max_us(0);
static std::atomic<int> g_gateway_ts_last_command(0);
static std::atomic<int> g_gateway_ts_parser_in_frame(0);
static std::atomic<uint32_t> g_gateway_ts_parser_remaining(0);
static std::atomic<uint64_t> g_gateway_ts_frame_to_usb_rx_us(0);
static std::atomic<uint64_t> g_gateway_ts_frame_to_usb_rx_max_us(0);

/* CPU idle/load */
static std::atomic<uint64_t> g_gateway_idle_ticks[2] = {0, 0};
static bool g_gateway_idle_hooks_registered = false;

struct GatewayDiagSample
{
    uint32_t rel_ms;
    uint32_t uptime_s;
    uint32_t active;
    uint32_t usb_ready;
    uint32_t cpu0_load_pct;
    uint32_t cpu1_load_pct;
    uint32_t free_heap;
    uint32_t largest_heap;
    uint32_t min_free_heap;
    int32_t sta_count;
    int32_t sta_rssi;

    int32_t tcp_fd;
    int32_t tcp_so_error;
    int32_t tcp_rcvbuf;
    int32_t tcp_sndbuf;
    int32_t tcp_nodelay;
    int32_t tcp_pending;
    uint32_t tcp_rx_bytes;
    uint32_t tcp_rx_chunks;
    uint32_t tcp_rx_eagain;
    uint32_t tcp_rx_errors;
    uint32_t tcp_recv_time_max_us;
    uint32_t tcp_rx_gap_max_us;
    uint32_t tcp_tx_bytes;
    uint32_t tcp_tx_calls;
    uint32_t tcp_tx_eagain;
    uint32_t tcp_tx_partial;
    uint32_t tcp_tx_faults;
    int32_t tcp_tx_fault_errno;
    uint32_t tcp_tx_gap_max_us;
    uint32_t tcp_tx_avg_us;
    uint32_t tcp_tx_max_us;
    uint32_t tcp_last_rx_age_us;
    uint32_t tcp_last_tx_age_us;

    uint32_t usb_rx_bytes;
    uint32_t usb_rx_chunks;
    uint32_t usb_rx_interval_us;
    uint32_t usb_rx_interval_max_us;
    uint32_t usb_tx_bytes;
    uint32_t usb_tx_count;
    uint32_t usb_tx_failures;
    uint32_t usb_tx_avg_us;
    uint32_t usb_tx_max_us;
    int32_t usb_tx_last_error;
    uint32_t usb_to_tcp_bytes;
    uint32_t usb_to_tcp_chunks;
    uint32_t usb_to_tcp_drop_bytes;
    uint32_t usb_to_tcp_queue_bytes;
    uint32_t usb_to_tcp_queue_delay_us;
    uint32_t usb_to_tcp_queue_delay_max_us;

    uint32_t ts_requests;
    uint32_t ts_request_interval_us;
    uint32_t ts_frames;
    uint32_t ts_legacy;
    uint32_t ts_frame_interval_us;
    uint32_t ts_frame_interval_min_us;
    uint32_t ts_frame_interval_max_us;
    uint32_t ts_frame_to_usb_rx_us;
    uint32_t ts_frame_to_usb_rx_max_us;
    int32_t ts_last_command;
    int32_t ts_parser_in_frame;
    uint32_t ts_parser_remaining;
    uint32_t ts_roundtrip_us;
    uint32_t ts_roundtrip_max_us;

    uint32_t http_requests;
    uint32_t http_connections;
    uint32_t http_req_rate_hz;
    uint32_t http_conn_rate_hz;

    uint32_t gw_hwm_words;
    uint32_t usb_mgr_hwm_words;
    uint32_t tcp_tx_hwm_words;
    uint32_t usb_host_hwm_words;
    uint32_t http_hwm_words;
    uint32_t json_hwm_words;
};

static GatewayDiagSample g_gateway_diag_storage[GATEWAY_DIAG_SAMPLE_COUNT] = {};
static std::atomic<uint32_t> g_gateway_diag_sample_count(0);
static std::atomic<bool> g_gateway_diag_capture_active(false);
static std::atomic<bool> g_gateway_diag_capture_ready(false);
static std::atomic<int32_t> g_gateway_diag_capture_error(0);
static std::atomic<uint64_t> g_gateway_diag_capture_start_us(0);
static uint32_t g_gateway_diag_http_req_base = 0;
static uint32_t g_gateway_diag_http_conn_base = 0;


/* ============================================================
   SESSION TIMING
   ============================================================ */

static std::atomic<uint64_t>
    g_gateway_session_start_us(0);

static std::atomic<uint64_t>
    g_gateway_session_tcp_rx_first_us(0);

static std::atomic<uint64_t>
    g_gateway_session_usb_rx_first_us(0);


/* ============================================================
   BUFFERS
   ============================================================ */

static uint8_t
    g_gateway_tcp_rx_buffer[
        GATEWAY_TCP_RX_BUFFER_SIZE];


/* ============================================================
   TIME HELPER
   ============================================================ */

static inline uint64_t gateway_now_us(void)
{
    return (uint64_t)esp_timer_get_time();
}


/* ============================================================
   ATOMIC MAX
   ============================================================ */

static inline void gateway_update_max(
    std::atomic<uint64_t> &maximum,
    uint64_t value)
{
    uint64_t old =
        maximum.load(
            std::memory_order_relaxed);

    while (
        value > old &&
        !maximum.compare_exchange_weak(
            old,
            value,
            std::memory_order_relaxed,
            std::memory_order_relaxed))
    {
    }
}


/* ============================================================
   RESET SESSION STATS
   ============================================================ */

static void gateway_reset_session_stats(void)
{
    g_gateway_usb_rx_bytes.store(
        0,
        std::memory_order_relaxed);

    g_gateway_tcp_rx_bytes.store(
        0,
        std::memory_order_relaxed);

    g_gateway_usb_rx_chunks.store(
        0,
        std::memory_order_relaxed);

    g_gateway_tcp_rx_chunks.store(
        0,
        std::memory_order_relaxed);

    g_gateway_usb_to_tcp_bytes.store(
        0,
        std::memory_order_relaxed);

    g_gateway_usb_to_tcp_chunks.store(
        0,
        std::memory_order_relaxed);

    g_gateway_usb_to_tcp_drop_bytes.store(
        0,
        std::memory_order_relaxed);

    g_gateway_tcp_tx_faults.store(
        0,
        std::memory_order_relaxed);

    g_gateway_tcp_tx_fault_errno.store(
        0,
        std::memory_order_relaxed);

    g_gateway_usb_tx_count.store(
        0,
        std::memory_order_relaxed);

    g_gateway_usb_tx_bytes.store(
        0,
        std::memory_order_relaxed);

    g_gateway_usb_tx_time_us.store(
        0,
        std::memory_order_relaxed);

    g_gateway_usb_tx_max_us.store(
        0,
        std::memory_order_relaxed);

    g_gateway_tcp_tx_count.store(
        0,
        std::memory_order_relaxed);

    g_gateway_tcp_tx_bytes.store(
        0,
        std::memory_order_relaxed);

    g_gateway_tcp_tx_time_us.store(
        0,
        std::memory_order_relaxed);

    g_gateway_tcp_tx_max_us.store(
        0,
        std::memory_order_relaxed);

    g_gateway_ts_requests.store(
        0,
        std::memory_order_relaxed);

    g_gateway_ts_request_interval_us.store(
        0,
        std::memory_order_relaxed);

    g_gateway_ts_request_last_us.store(
        0,
        std::memory_order_relaxed);

    g_gateway_ts_roundtrip_us.store(
        0,
        std::memory_order_relaxed);

    g_gateway_ts_roundtrip_max_us.store(
        0,
        std::memory_order_relaxed);

    g_gateway_ts_usb_rx_count.store(
        0,
        std::memory_order_relaxed);

    g_gateway_ts_usb_rx_interval_us.store(
        0,
        std::memory_order_relaxed);

    g_gateway_ts_usb_rx_last_us.store(
        0,
        std::memory_order_relaxed);

    g_gateway_tcp_rx_eagain.store(0, std::memory_order_relaxed);
    g_gateway_tcp_rx_errors.store(0, std::memory_order_relaxed);
    g_gateway_tcp_tx_eagain.store(0, std::memory_order_relaxed);
    g_gateway_tcp_tx_partial.store(0, std::memory_order_relaxed);
    g_gateway_tcp_recv_time_max_us.store(0, std::memory_order_relaxed);
    g_gateway_tcp_rx_gap_max_us.store(0, std::memory_order_relaxed);
    g_gateway_tcp_tx_gap_max_us.store(0, std::memory_order_relaxed);
    g_gateway_tcp_last_rx_us.store(0, std::memory_order_relaxed);
    g_gateway_tcp_last_tx_us.store(0, std::memory_order_relaxed);
    g_gateway_tcp_last_socket_error.store(0, std::memory_order_relaxed);
    g_gateway_usb_tx_failures.store(0, std::memory_order_relaxed);
    g_gateway_usb_tx_last_error.store(0, std::memory_order_relaxed);
    g_gateway_usb_rx_interval_max_us.store(0, std::memory_order_relaxed);
    g_gateway_usb_to_tcp_queue_delay_us.store(0, std::memory_order_relaxed);
    g_gateway_usb_to_tcp_queue_delay_max_us.store(0, std::memory_order_relaxed);
    g_gateway_usb_to_tcp_last_enqueue_us.store(0, std::memory_order_relaxed);
    g_gateway_ts_frames.store(0, std::memory_order_relaxed);
    g_gateway_ts_legacy.store(0, std::memory_order_relaxed);
    g_gateway_ts_frame_last_us.store(0, std::memory_order_relaxed);
    g_gateway_ts_frame_interval_us.store(0, std::memory_order_relaxed);
    g_gateway_ts_frame_interval_sum_us.store(0, std::memory_order_relaxed);
    g_gateway_ts_frame_interval_samples.store(0, std::memory_order_relaxed);
    g_gateway_ts_frame_interval_min_us.store(0, std::memory_order_relaxed);
    g_gateway_ts_frame_interval_max_us.store(0, std::memory_order_relaxed);
    g_gateway_ts_last_command.store(0, std::memory_order_relaxed);
    g_gateway_ts_parser_in_frame.store(0, std::memory_order_relaxed);
    g_gateway_ts_parser_remaining.store(0, std::memory_order_relaxed);
    g_gateway_ts_frame_to_usb_rx_us.store(0, std::memory_order_relaxed);
    g_gateway_ts_frame_to_usb_rx_max_us.store(0, std::memory_order_relaxed);

    g_gateway_session_start_us.store(
        gateway_now_us(),
        std::memory_order_relaxed);

    g_gateway_session_tcp_rx_first_us.store(
        0,
        std::memory_order_relaxed);

    g_gateway_session_usb_rx_first_us.store(
        0,
        std::memory_order_relaxed);
}


/* ============================================================
   SESSION SUMMARY
   ============================================================ */

static void gateway_print_session_stats(void)
{
    const uint64_t now_us =
        gateway_now_us();

    const uint64_t start_us =
        g_gateway_session_start_us.load(
            std::memory_order_relaxed);

    const uint64_t elapsed_us =
        (now_us >= start_us)
            ? (now_us - start_us)
            : 0;

    const uint64_t usb_tx_count =
        g_gateway_usb_tx_count.load(
            std::memory_order_relaxed);

    const uint64_t usb_tx_bytes =
        g_gateway_usb_tx_bytes.load(
            std::memory_order_relaxed);

    const uint64_t usb_tx_time_us =
        g_gateway_usb_tx_time_us.load(
            std::memory_order_relaxed);

    const uint64_t usb_tx_max_us =
        g_gateway_usb_tx_max_us.load(
            std::memory_order_relaxed);

    const uint64_t tcp_tx_count =
        g_gateway_tcp_tx_count.load(
            std::memory_order_relaxed);

    const uint64_t tcp_tx_bytes =
        g_gateway_tcp_tx_bytes.load(
            std::memory_order_relaxed);

    const uint64_t tcp_tx_time_us =
        g_gateway_tcp_tx_time_us.load(
            std::memory_order_relaxed);

    const uint64_t tcp_tx_max_us =
        g_gateway_tcp_tx_max_us.load(
            std::memory_order_relaxed);

    const uint64_t usb_rx_bytes =
        g_gateway_usb_rx_bytes.load(
            std::memory_order_relaxed);

    const uint64_t usb_rx_chunks =
        g_gateway_usb_rx_chunks.load(
            std::memory_order_relaxed);

    const uint64_t tcp_rx_bytes =
        g_gateway_tcp_rx_bytes.load(
            std::memory_order_relaxed);

    const uint64_t tcp_rx_chunks =
        g_gateway_tcp_rx_chunks.load(
            std::memory_order_relaxed);

    const uint64_t usb_to_tcp_chunks =
        g_gateway_usb_to_tcp_chunks.load(
            std::memory_order_relaxed);

    const uint64_t usb_tx_avg_us =
        (usb_tx_count != 0)
            ? (usb_tx_time_us / usb_tx_count)
            : 0;

    const uint64_t usb_tx_avg_bytes =
        (usb_tx_count != 0)
            ? (usb_tx_bytes / usb_tx_count)
            : 0;

    const uint64_t tcp_tx_avg_us =
        (tcp_tx_count != 0)
            ? (tcp_tx_time_us / tcp_tx_count)
            : 0;

    const uint64_t tcp_tx_avg_bytes =
        (tcp_tx_count != 0)
            ? (tcp_tx_bytes / tcp_tx_count)
            : 0;

    uint64_t usb_tx_bps = 0;
    uint64_t tcp_tx_bps = 0;

    if (elapsed_us > 0)
    {
        usb_tx_bps =
            (usb_tx_bytes * 1000000ULL) /
            elapsed_us;

        tcp_tx_bps =
            (tcp_tx_bytes * 1000000ULL) /
            elapsed_us;
    }

    printf(
        "\r\n"
        "====================================================\r\n"
        "[GATEWAY] SESSION PERFORMANCE\r\n"
        "----------------------------------------------------\r\n"
        "SESSION TIME        : %" PRIu64 " ms\r\n"
        "\r\n"
        "TCP RX BYTES        : %" PRIu64 "\r\n"
        "TCP RX CHUNKS       : %" PRIu64 "\r\n"
        "\r\n"
        "USB RX BYTES        : %" PRIu64 "\r\n"
        "USB RX CHUNKS       : %" PRIu64 "\r\n"
        "USB -> TCP CHUNKS   : %" PRIu64 "\r\n"
        "\r\n"
        "USB TX TRANSFERS    : %" PRIu64 "\r\n"
        "USB TX BYTES        : %" PRIu64 "\r\n"
        "USB TX AVG SIZE     : %" PRIu64 " bytes\r\n"
        "USB TX AVG TIME     : %" PRIu64 " us\r\n"
        "USB TX MAX TIME     : %" PRIu64 " us\r\n"
        "USB TX RATE         : %" PRIu64 " B/s\r\n"
        "\r\n"
        "TCP TX CALLS        : %" PRIu64 "\r\n"
        "TCP TX BYTES        : %" PRIu64 "\r\n"
        "TCP TX AVG SIZE     : %" PRIu64 " bytes\r\n"
        "TCP TX AVG TIME     : %" PRIu64 " us\r\n"
        "TCP TX MAX TIME     : %" PRIu64 " us\r\n"
        "TCP TX RATE         : %" PRIu64 " B/s\r\n"
        "\r\n"
        "TS REQUESTS         : %" PRIu64 "\r\n"
        "TS REQUEST INTERVAL : %" PRIu64 " us\r\n"
        "TS ROUND TRIP       : %" PRIu64 " us\r\n"
        "TS ROUND TRIP MAX   : %" PRIu64 " us\r\n"
        "====================================================\r\n",

        elapsed_us / 1000ULL,

        tcp_rx_bytes,
        tcp_rx_chunks,

        usb_rx_bytes,
        usb_rx_chunks,
        usb_to_tcp_chunks,

        usb_tx_count,
        usb_tx_bytes,
        usb_tx_avg_bytes,
        usb_tx_avg_us,
        usb_tx_max_us,
        usb_tx_bps,

        tcp_tx_count,
        tcp_tx_bytes,
        tcp_tx_avg_bytes,
        tcp_tx_avg_us,
        tcp_tx_max_us,
        tcp_tx_bps,

        g_gateway_ts_requests.load(
            std::memory_order_relaxed),

        g_gateway_ts_request_interval_us.load(
            std::memory_order_relaxed),

        g_gateway_ts_roundtrip_us.load(
            std::memory_order_relaxed),

        g_gateway_ts_roundtrip_max_us.load(
            std::memory_order_relaxed)
    );
}


/* ============================================================
   USB RX CALLBACK
   ============================================================ */

extern "C"
bool gateway_vcp_rx_callback(
    const uint8_t *data,
    size_t data_len,
    void *arg)
{
    (void)arg;

    if (!g_gateway_usb_device_ready.load(std::memory_order_relaxed) ||
        data == nullptr ||
        data_len == 0)
    {
        return true;
    }

    const uint64_t usb_rx_now = gateway_now_us();
    const uint64_t previous_usb_rx =
        g_gateway_ts_usb_rx_last_us.exchange(
            usb_rx_now,
            std::memory_order_relaxed);

    if (previous_usb_rx != 0 && usb_rx_now >= previous_usb_rx)
    {
        const uint64_t interval = usb_rx_now - previous_usb_rx;
        g_gateway_ts_usb_rx_interval_us.store(
            interval,
            std::memory_order_relaxed);
        gateway_update_max(
            g_gateway_usb_rx_interval_max_us,
            interval);
    }

    g_gateway_ts_usb_rx_count.fetch_add(
        1,
        std::memory_order_relaxed);

    uint64_t zero = 0;
    (void)g_gateway_session_usb_rx_first_us.compare_exchange_strong(
        zero,
        usb_rx_now,
        std::memory_order_relaxed,
        std::memory_order_relaxed);

    const uint64_t request_time =
        g_gateway_ts_request_last_us.load(
            std::memory_order_relaxed);

    if (request_time != 0 && usb_rx_now >= request_time)
    {
        const uint64_t roundtrip = usb_rx_now - request_time;
        g_gateway_ts_roundtrip_us.store(
            roundtrip,
            std::memory_order_relaxed);
        gateway_update_max(
            g_gateway_ts_roundtrip_max_us,
            roundtrip);
    }

    g_gateway_usb_rx_bytes.fetch_add(
        (uint64_t)data_len,
        std::memory_order_relaxed);

    g_gateway_usb_rx_chunks.fetch_add(
        1,
        std::memory_order_relaxed);

    /* BrickOS owns the USB CDC transaction. Callback never waits on Wi-Fi or
       HTTP; the protocol task consumes bytes from a FreeRTOS stream buffer. */
    if (g_brick_usb_transaction.load(std::memory_order_acquire))
    {
        if (g_brick_usb_rx_stream == nullptr ||
            xStreamBufferSend(g_brick_usb_rx_stream, data, data_len, 0) != data_len)
        {
            g_gateway_usb_to_tcp_drop_bytes.fetch_add(
                static_cast<uint64_t>(data_len), std::memory_order_relaxed);
        }
        return true;
    }

    /* Dedicated transport: USB RX goes directly to the TCP socket. */
    const int client = g_gateway_active_client_socket.load(
        std::memory_order_acquire);

    if (client < 0)
    {
        g_gateway_usb_to_tcp_drop_bytes.fetch_add(
            (uint64_t)data_len,
            std::memory_order_relaxed);
        return true;
    }

    const uint64_t send_start_us = gateway_now_us();
    size_t sent_total = 0;

    while (sent_total < data_len)
    {
        const ssize_t sent = send(
            client,
            data + sent_total,
            data_len - sent_total,
            MSG_DONTWAIT);

        if (sent > 0)
        {
            sent_total += (size_t)sent;
            continue;
        }

        if (sent < 0 &&
            (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            taskYIELD();
            continue;
        }

        const int send_errno = (sent < 0) ? errno : EIO;
        g_gateway_tcp_tx_faults.fetch_add(
            1,
            std::memory_order_relaxed);
        g_gateway_tcp_tx_fault_errno.store(
            send_errno,
            std::memory_order_relaxed);
        g_gateway_tcp_last_socket_error.store(
            send_errno,
            std::memory_order_relaxed);
        break;
    }

    const uint64_t send_done_us = gateway_now_us();
    const uint64_t send_elapsed_us = send_done_us - send_start_us;

    g_gateway_tcp_tx_count.fetch_add(
        1,
        std::memory_order_relaxed);
    g_gateway_tcp_tx_bytes.fetch_add(
        (uint64_t)sent_total,
        std::memory_order_relaxed);
    g_gateway_tcp_tx_time_us.fetch_add(
        send_elapsed_us,
        std::memory_order_relaxed);
    gateway_update_max(
        g_gateway_tcp_tx_max_us,
        send_elapsed_us);

    const uint64_t previous_tx =
        g_gateway_tcp_last_tx_us.exchange(
            send_done_us,
            std::memory_order_relaxed);

    if (previous_tx != 0 && send_done_us >= previous_tx)
    {
        gateway_update_max(
            g_gateway_tcp_tx_gap_max_us,
            send_done_us - previous_tx);
    }

    g_gateway_usb_to_tcp_bytes.fetch_add(
        (uint64_t)sent_total,
        std::memory_order_relaxed);
    g_gateway_usb_to_tcp_chunks.fetch_add(
        1,
        std::memory_order_relaxed);

    /* No queue exists in the dedicated transport path. */
    g_gateway_usb_to_tcp_queue_delay_us.store(
        0,
        std::memory_order_relaxed);

    if (sent_total < data_len)
    {
        g_gateway_usb_to_tcp_drop_bytes.fetch_add(
            (uint64_t)(data_len - sent_total),
            std::memory_order_relaxed);
    }

    return true;
}

/* ============================================================
   USB EVENTS
   ============================================================ */

static void gateway_vcp_event_callback(
    const cdc_acm_host_dev_event_data_t *event,
    void *user_ctx)
{
    (void)user_ctx;

    if (event == nullptr)
        return;

    switch (event->type)
    {
        case CDC_ACM_HOST_DEVICE_DISCONNECTED:

            g_brick_usb_disconnects.fetch_add(1, std::memory_order_relaxed);
            ESP_LOGI(
                GATEWAY_TAG,
                "USB disconnected");

            g_gateway_usb_device_ready.store(
                false,
                std::memory_order_relaxed);

            if (
                g_gateway_device_disconnected_sem
                != NULL)
            {
                xSemaphoreGive(
                    g_gateway_device_disconnected_sem);
            }

            break;

        case CDC_ACM_HOST_SERIAL_STATE:

            ESP_LOGD(
                GATEWAY_TAG,
                "USB serial state 0x%04X",
                event->data.serial_state.val);

            break;

        case CDC_ACM_HOST_NETWORK_CONNECTION:

            ESP_LOGD(
                GATEWAY_TAG,
                "USB network state %s",
                event->data.network_connected
                    ? "connected"
                    : "disconnected");

            break;

        default:

            break;
    }
}


/* ============================================================
   USB HOST TASK
   ============================================================ */

static void gateway_usb_host_task(void *arg)
{
    (void)arg;

    while (true)
    {
        uint32_t flags = 0;

        esp_err_t err =
            usb_host_lib_handle_events(
                pdMS_TO_TICKS(100),
                &flags);

        if (
            err != ESP_OK &&
            err != ESP_ERR_INVALID_STATE)
        {
            ESP_LOGW(
                GATEWAY_TAG,
                "USB host event handling: 0x%x",
                err);
        }

        // Do not call usb_host_device_free_all() in the active host event loop.
        // CDC-ACM owns its device handles and releases them on disconnect.
        (void)flags;
        // Always allow CPU0 idle and Wi-Fi servicing; 10ms is a full FreeRTOS tick at 100Hz.
        const TickType_t idle_ticks = pdMS_TO_TICKS(10);
        vTaskDelay(idle_ticks > 0 ? idle_ticks : 1);
    }
}


/* ============================================================
   OPEN USB DEVICE
   ============================================================ */

static bool gateway_open_usb_device(void)
{
    g_brick_usb_open_attempts.fetch_add(1, std::memory_order_relaxed);
    // Brick ECU STM32F7 native USB CDC-ACM: VID 0483, PID 5740.
    // Do not run the generic CH34x/CP210x/FTDI VCP scanner on each retry;
    // that is unrelated to the known ECU and complicates host enumeration.
    cdc_acm_host_device_config_t dev_cfg = {
        .connection_timeout_ms = 1000,
        .out_buffer_size = GATEWAY_USB_BUFFER_SIZE,
        .in_buffer_size = GATEWAY_USB_BUFFER_SIZE,
        .event_cb = gateway_vcp_event_callback,
        .data_cb = gateway_vcp_rx_callback,
        .user_arg = NULL
    };
    g_gateway_usb_device.cdc_handle = nullptr;
    esp_err_t err = cdc_acm_host_open(0x0483, 0x5740, 1, &dev_cfg,
                                     &g_gateway_usb_device.cdc_handle);
    if (err == ESP_OK && g_gateway_usb_device.cdc_handle != nullptr) {
        g_gateway_usb_device.is_vcp_device = false;
        g_gateway_usb_device.need_close = true;
        g_gateway_usb_device_ready.store(true, std::memory_order_release);
        g_brick_usb_successful_opens.fetch_add(1, std::memory_order_relaxed);
        ESP_LOGI(GATEWAY_TAG, "STM32 native CDC-ACM opened (0483:5740)");
        return true;
    }
    g_gateway_usb_device.cdc_handle = nullptr;
    g_gateway_usb_device.need_close = false;
    g_brick_usb_open_failures.fetch_add(1, std::memory_order_relaxed);
    // Device not present is normal during boot; retries are handled by manager.
    if (err != ESP_ERR_NOT_FOUND && err != ESP_ERR_TIMEOUT)
        ESP_LOGW(GATEWAY_TAG, "STM32 CDC open failed: %s", esp_err_to_name(err));
    return false;
}


/* ============================================================
   CLOSE USB DEVICE
   ============================================================ */

static void gateway_close_usb_device(void)
{
    g_gateway_usb_device_ready.store(
        false,
        std::memory_order_relaxed);

    if (!g_gateway_usb_device.need_close)
        return;

    if (g_gateway_usb_device.is_vcp_device)
    {
        if (
            g_gateway_usb_device.vcp_device
            != nullptr)
        {
            delete
                g_gateway_usb_device.vcp_device;

            g_gateway_usb_device.vcp_device =
                nullptr;
        }
    }
    else if (
        g_gateway_usb_device.cdc_handle
        != nullptr)
    {
        cdc_acm_host_close(
            g_gateway_usb_device.cdc_handle);

        g_gateway_usb_device.cdc_handle =
            nullptr;
    }

    g_gateway_usb_device.is_vcp_device =
        false;

    g_gateway_usb_device.need_close =
        false;
}


/* ============================================================
   USB LINE CODING
   ============================================================ */

static void gateway_setup_usb_baudrate(
    uint32_t baudrate)
{
    if (
        !g_gateway_usb_device.need_close)
    {
        return;
    }

    cdc_acm_line_coding_t line_coding = {

        .dwDTERate =
            baudrate,

        .bCharFormat =
            0,

        .bParityType =
            0,

        .bDataBits =
            8
    };


    esp_err_t result = ESP_FAIL;


    if (
        g_gateway_usb_device.is_vcp_device &&
        g_gateway_usb_device.vcp_device
            != nullptr)
    {
        result =
            g_gateway_usb_device.vcp_device
                ->line_coding_set(
                    &line_coding);

        g_gateway_usb_device.vcp_device
            ->set_control_line_state(
                true,
                true);
    }
    else if (
        !g_gateway_usb_device.is_vcp_device &&
        g_gateway_usb_device.cdc_handle
            != nullptr)
    {
        result =
            cdc_acm_host_line_coding_set(
                g_gateway_usb_device.cdc_handle,
                &line_coding);

        cdc_acm_host_set_control_line_state(
            g_gateway_usb_device.cdc_handle,
            true,
            true);
    }


    ESP_LOGI(
        GATEWAY_TAG,
        "USB baud request %lu result=0x%x",
        (unsigned long)baudrate,
        (unsigned)result);
}


static bool gateway_prepare_data_tasks(void);
static bool gateway_start_usb_stack(void);


/* ============================================================
   CAPTURE-ONLY TUNERSTUDIO PARSER + CPU IDLE HOOKS
   ============================================================ */

static void gateway_ts_frame_event(uint8_t command, uint64_t ts_us, void *ctx)
{
    (void)ctx;
    g_gateway_ts_last_command.store((int)command, std::memory_order_relaxed);

    if (command == (uint8_t)'H' || command == (uint8_t)'O') {
        g_gateway_ts_legacy.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const uint64_t prev = g_gateway_ts_frame_last_us.exchange(ts_us, std::memory_order_relaxed);
    if (prev != 0 && ts_us >= prev) {
        const uint64_t interval = ts_us - prev;
        g_gateway_ts_frame_interval_us.store(interval, std::memory_order_relaxed);
        g_gateway_ts_frame_interval_sum_us.fetch_add(interval, std::memory_order_relaxed);
        g_gateway_ts_frame_interval_samples.fetch_add(1, std::memory_order_relaxed);
        uint64_t min_old = g_gateway_ts_frame_interval_min_us.load(std::memory_order_relaxed);
        if (min_old == 0 || interval < min_old)
            g_gateway_ts_frame_interval_min_us.store(interval, std::memory_order_relaxed);
        gateway_update_max(g_gateway_ts_frame_interval_max_us, interval);
    }
    g_gateway_ts_frames.fetch_add(1, std::memory_order_relaxed);
}

static bool gateway_idle_hook_cpu0(void)
{
    g_gateway_idle_ticks[0].fetch_add(1, std::memory_order_relaxed);
    return true;
}

static bool gateway_idle_hook_cpu1(void)
{
    g_gateway_idle_ticks[1].fetch_add(1, std::memory_order_relaxed);
    return true;
}

static void gateway_register_idle_hooks(void)
{
    if (g_gateway_idle_hooks_registered)
        return;
    esp_err_t a = esp_register_freertos_idle_hook_for_cpu(gateway_idle_hook_cpu0, 0);
    esp_err_t b = esp_register_freertos_idle_hook_for_cpu(gateway_idle_hook_cpu1, 1);
    if (a == ESP_OK && b == ESP_OK) {
        g_gateway_idle_hooks_registered = true;
    } else {
        ESP_LOGW(GATEWAY_TAG, "idle hook registration failed cpu0=0x%x cpu1=0x%x", a, b);
    }
}


static uint32_t gateway_cpu_load_from_idle(uint64_t idle_ticks)
{
    const uint64_t expected = ((uint64_t)configTICK_RATE_HZ * GATEWAY_DIAG_SAMPLE_PERIOD_MS) / 1000ULL;
    if (expected == 0) return 0;
    if (idle_ticks >= expected) return 0;
    return (uint32_t)(((expected - idle_ticks) * 100ULL) / expected);
}

static void gateway_diag_capture_reset(void)
{
    tsdiag_init(&g_gateway_ts_parser);
    g_gateway_diag_sample_count.store(0, std::memory_order_relaxed);
    g_gateway_diag_capture_ready.store(false, std::memory_order_release);
    g_gateway_diag_capture_start_us.store(gateway_now_us(), std::memory_order_relaxed);
    g_gateway_diag_http_req_base = g_http_requests;
    g_gateway_diag_http_conn_base = g_http_connections;
    g_gateway_ts_frames.store(0, std::memory_order_relaxed);
    g_gateway_ts_legacy.store(0, std::memory_order_relaxed);
    g_gateway_ts_frame_last_us.store(0, std::memory_order_relaxed);
    g_gateway_ts_frame_interval_us.store(0, std::memory_order_relaxed);
    g_gateway_ts_frame_interval_sum_us.store(0, std::memory_order_relaxed);
    g_gateway_ts_frame_interval_samples.store(0, std::memory_order_relaxed);
    g_gateway_ts_frame_interval_min_us.store(0, std::memory_order_relaxed);
    g_gateway_ts_frame_interval_max_us.store(0, std::memory_order_relaxed);
    g_gateway_ts_last_command.store(0, std::memory_order_relaxed);
    g_gateway_ts_parser_in_frame.store(0, std::memory_order_relaxed);
    g_gateway_ts_parser_remaining.store(0, std::memory_order_relaxed);
    g_gateway_ts_frame_to_usb_rx_us.store(0, std::memory_order_relaxed);
    g_gateway_ts_frame_to_usb_rx_max_us.store(0, std::memory_order_relaxed);
    g_gateway_idle_ticks[0].store(0, std::memory_order_relaxed);
    g_gateway_idle_ticks[1].store(0, std::memory_order_relaxed);
}

static void gateway_diag_fill_sample(GatewayDiagSample &d, uint64_t now_us, uint64_t sample_prev_us,
                                     uint64_t prev_tcp_rx, uint64_t prev_tcp_tx, uint64_t prev_usb_rx,
                                     uint64_t prev_usb_tx, uint64_t prev_usb_to_tcp, uint64_t prev_ts_req,
                                     uint64_t prev_ts_frame, uint32_t prev_http_req, uint32_t prev_http_conn)
{
    memset(&d, 0, sizeof(d));
    const uint64_t cap_start = g_gateway_diag_capture_start_us.load(std::memory_order_relaxed);
    d.rel_ms = (uint32_t)((now_us >= cap_start) ? ((now_us - cap_start) / 1000ULL) : 0);
    d.uptime_s = (uint32_t)(now_us / 1000000ULL);
    d.active = g_gateway_bridge_active.load(std::memory_order_acquire) ? 1U : 0U;
    d.usb_ready = g_gateway_usb_device_ready.load(std::memory_order_acquire) ? 1U : 0U;
    d.cpu0_load_pct = gateway_cpu_load_from_idle(g_gateway_idle_ticks[0].exchange(0, std::memory_order_relaxed));
    d.cpu1_load_pct = gateway_cpu_load_from_idle(g_gateway_idle_ticks[1].exchange(0, std::memory_order_relaxed));
    d.free_heap = (uint32_t)esp_get_free_heap_size();
    d.largest_heap = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    d.min_free_heap = (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);

    wifi_sta_list_t sta_list = {};
    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
        d.sta_count = sta_list.num;
        d.sta_rssi = (sta_list.num > 0) ? sta_list.sta[0].rssi : 0;
    }

    d.tcp_fd = -1;
    d.tcp_so_error = 0;
    d.tcp_rcvbuf = 0;
    d.tcp_sndbuf = 0;
    d.tcp_nodelay = 0;
    d.tcp_pending = 0;
    const int fd = g_gateway_active_client_socket.load(std::memory_order_acquire);
    d.tcp_fd = fd;
    if (fd >= 0) {
        socklen_t sl = sizeof(int);
        (void)getsockopt(fd, SOL_SOCKET, SO_ERROR, &d.tcp_so_error, &sl);
        (void)getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &d.tcp_rcvbuf, &sl);
        (void)getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &d.tcp_sndbuf, &sl);
        (void)getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &d.tcp_nodelay, &sl);
        int pending = 0;
        if (ioctl(fd, FIONREAD, &pending) == 0)
            d.tcp_pending = pending;
    }

    d.tcp_rx_bytes = g_gateway_tcp_rx_bytes.load(std::memory_order_relaxed);
    d.tcp_rx_chunks = g_gateway_tcp_rx_chunks.load(std::memory_order_relaxed);
    d.tcp_rx_eagain = g_gateway_tcp_rx_eagain.load(std::memory_order_relaxed);
    d.tcp_rx_errors = g_gateway_tcp_rx_errors.load(std::memory_order_relaxed);
    d.tcp_recv_time_max_us = g_gateway_tcp_recv_time_max_us.load(std::memory_order_relaxed);
    d.tcp_rx_gap_max_us = g_gateway_tcp_rx_gap_max_us.load(std::memory_order_relaxed);
    d.tcp_tx_bytes = g_gateway_tcp_tx_bytes.load(std::memory_order_relaxed);
    d.tcp_tx_calls = g_gateway_tcp_tx_count.load(std::memory_order_relaxed);
    d.tcp_tx_eagain = g_gateway_tcp_tx_eagain.load(std::memory_order_relaxed);
    d.tcp_tx_partial = g_gateway_tcp_tx_partial.load(std::memory_order_relaxed);
    d.tcp_tx_faults = g_gateway_tcp_tx_faults.load(std::memory_order_relaxed);
    d.tcp_tx_fault_errno = g_gateway_tcp_tx_fault_errno.load(std::memory_order_relaxed);
    d.tcp_tx_gap_max_us = g_gateway_tcp_tx_gap_max_us.load(std::memory_order_relaxed);
    d.tcp_tx_avg_us = d.tcp_tx_calls ? (g_gateway_tcp_tx_time_us.load(std::memory_order_relaxed) / d.tcp_tx_calls) : 0;
    d.tcp_tx_max_us = g_gateway_tcp_tx_max_us.load(std::memory_order_relaxed);
    const uint64_t lr = g_gateway_tcp_last_rx_us.load(std::memory_order_relaxed);
    const uint64_t lt = g_gateway_tcp_last_tx_us.load(std::memory_order_relaxed);
    d.tcp_last_rx_age_us = lr && now_us >= lr ? now_us - lr : 0;
    d.tcp_last_tx_age_us = lt && now_us >= lt ? now_us - lt : 0;

    d.usb_rx_bytes = g_gateway_usb_rx_bytes.load(std::memory_order_relaxed);
    d.usb_rx_chunks = g_gateway_usb_rx_chunks.load(std::memory_order_relaxed);
    d.usb_rx_interval_us = g_gateway_ts_usb_rx_interval_us.load(std::memory_order_relaxed);
    d.usb_rx_interval_max_us = g_gateway_usb_rx_interval_max_us.load(std::memory_order_relaxed);
    d.usb_tx_bytes = g_gateway_usb_tx_bytes.load(std::memory_order_relaxed);
    d.usb_tx_count = g_gateway_usb_tx_count.load(std::memory_order_relaxed);
    d.usb_tx_failures = g_gateway_usb_tx_failures.load(std::memory_order_relaxed);
    d.usb_tx_avg_us = g_gateway_usb_tx_count.load(std::memory_order_relaxed) ? (g_gateway_usb_tx_time_us.load(std::memory_order_relaxed) / g_gateway_usb_tx_count.load(std::memory_order_relaxed)) : 0;
    d.usb_tx_max_us = g_gateway_usb_tx_max_us.load(std::memory_order_relaxed);
    d.usb_tx_last_error = g_gateway_usb_tx_last_error.load(std::memory_order_relaxed);
    d.usb_to_tcp_bytes = g_gateway_usb_to_tcp_bytes.load(std::memory_order_relaxed);
    d.usb_to_tcp_chunks = g_gateway_usb_to_tcp_chunks.load(std::memory_order_relaxed);
    d.usb_to_tcp_drop_bytes = g_gateway_usb_to_tcp_drop_bytes.load(std::memory_order_relaxed);
    d.usb_to_tcp_queue_bytes = 0;
    d.usb_to_tcp_queue_delay_us = g_gateway_usb_to_tcp_queue_delay_us.load(std::memory_order_relaxed);
    d.usb_to_tcp_queue_delay_max_us = g_gateway_usb_to_tcp_queue_delay_max_us.load(std::memory_order_relaxed);

    d.ts_requests = g_gateway_ts_requests.load(std::memory_order_relaxed);
    d.ts_request_interval_us = g_gateway_ts_request_interval_us.load(std::memory_order_relaxed);
    d.ts_frames = g_gateway_ts_frames.load(std::memory_order_relaxed);
    d.ts_legacy = g_gateway_ts_legacy.load(std::memory_order_relaxed);
    d.ts_frame_interval_us = g_gateway_ts_frame_interval_us.load(std::memory_order_relaxed);
    d.ts_frame_interval_min_us = g_gateway_ts_frame_interval_min_us.load(std::memory_order_relaxed);
    d.ts_frame_interval_max_us = g_gateway_ts_frame_interval_max_us.load(std::memory_order_relaxed);
    d.ts_frame_to_usb_rx_us = g_gateway_ts_frame_to_usb_rx_us.load(std::memory_order_relaxed);
    d.ts_frame_to_usb_rx_max_us = g_gateway_ts_frame_to_usb_rx_max_us.load(std::memory_order_relaxed);
    d.ts_last_command = g_gateway_ts_last_command.load(std::memory_order_relaxed);
    d.ts_parser_in_frame = g_gateway_ts_parser_in_frame.load(std::memory_order_relaxed);
    d.ts_parser_remaining = g_gateway_ts_parser_remaining.load(std::memory_order_relaxed);
    d.ts_roundtrip_us = g_gateway_ts_roundtrip_us.load(std::memory_order_relaxed);
    d.ts_roundtrip_max_us = g_gateway_ts_roundtrip_max_us.load(std::memory_order_relaxed);

    d.http_requests = g_http_requests;
    d.http_connections = g_http_connections;
    const uint64_t dt = (sample_prev_us && now_us > sample_prev_us) ? now_us - sample_prev_us : 0;
    const uint64_t denom = dt ? dt : 1;
    d.http_req_rate_hz = (uint32_t)(((uint64_t)(d.http_requests - prev_http_req) * 1000000ULL) / denom);
    d.http_conn_rate_hz = (uint32_t)(((uint64_t)(d.http_connections - prev_http_conn) * 1000000ULL) / denom);

    d.gw_hwm_words = g_gateway_tcp_server_task_handle ? uxTaskGetStackHighWaterMark(g_gateway_tcp_server_task_handle) : 0;
    d.usb_mgr_hwm_words = g_gateway_usb_mgr_task_handle ? uxTaskGetStackHighWaterMark(g_gateway_usb_mgr_task_handle) : 0;
    d.tcp_tx_hwm_words = g_gateway_tcp_server_task_handle ? uxTaskGetStackHighWaterMark(g_gateway_tcp_server_task_handle) : 0;
    d.usb_host_hwm_words = g_gateway_usb_host_task_handle ? uxTaskGetStackHighWaterMark(g_gateway_usb_host_task_handle) : 0;
    d.http_hwm_words = hub_web_http_stack_hwm_words();
    d.json_hwm_words = hub_web_json_cache_stack_hwm_words();

    (void)prev_tcp_rx; (void)prev_tcp_tx; (void)prev_usb_rx; (void)prev_usb_tx; (void)prev_usb_to_tcp; (void)prev_ts_req; (void)prev_ts_frame;
}

static GatewayDiagSample *gateway_diag_sample_ptr(uint32_t index)
{
    if (index >= GATEWAY_DIAG_SAMPLE_COUNT)
        return nullptr;
    return &g_gateway_diag_storage[index];
}

static bool gateway_diag_storage_allocated(void)
{
    return true; /* fixed BSS storage; never allocate/free during a TS session */
}

static void gateway_diag_storage_free(void)
{
    /* Intentionally empty.  Capture storage is static so diagnostics cannot
     * fragment or exhaust the heap used by TunerStudio. */
}

static uint64_t g_gateway_diag_next_sample_us = 0;
static uint64_t g_gateway_diag_prev_tcp_rx = 0;
static uint64_t g_gateway_diag_prev_tcp_tx = 0;
static uint64_t g_gateway_diag_prev_usb_rx = 0;
static uint64_t g_gateway_diag_prev_usb_tx = 0;
static uint64_t g_gateway_diag_prev_usb_to_tcp = 0;
static uint64_t g_gateway_diag_prev_ts_req = 0;
static uint64_t g_gateway_diag_prev_ts_frame = 0;
static uint32_t g_gateway_diag_prev_http_req = 0;
static uint32_t g_gateway_diag_prev_http_conn = 0;
static uint64_t g_gateway_diag_prev_sample_us = 0;

static void gateway_diag_capture_service(uint64_t now_us)
{
    if (!g_gateway_diag_capture_active.load(std::memory_order_acquire))
        return;

    if (g_gateway_diag_next_sample_us == 0) {
        g_gateway_diag_next_sample_us = g_gateway_diag_capture_start_us.load(std::memory_order_relaxed);
        g_gateway_diag_prev_sample_us = 0;
    }

    if (now_us < g_gateway_diag_next_sample_us)
        return;

    const uint32_t i = g_gateway_diag_sample_count.load(std::memory_order_acquire);
    if (i >= GATEWAY_DIAG_SAMPLE_COUNT) {
        g_gateway_diag_capture_active.store(false, std::memory_order_release);
        g_gateway_diag_capture_ready.store(true, std::memory_order_release);
        return;
    }

    GatewayDiagSample *dp = gateway_diag_sample_ptr(i);
    if (!dp) {
        g_gateway_diag_capture_active.store(false, std::memory_order_release);
        g_gateway_diag_capture_ready.store(false, std::memory_order_release);
        g_gateway_diag_capture_error.store(4, std::memory_order_release);
        return;
    }

    GatewayDiagSample &d = *dp;
    gateway_diag_fill_sample(
        d,
        now_us,
        g_gateway_diag_prev_sample_us,
        g_gateway_diag_prev_tcp_rx,
        g_gateway_diag_prev_tcp_tx,
        g_gateway_diag_prev_usb_rx,
        g_gateway_diag_prev_usb_tx,
        g_gateway_diag_prev_usb_to_tcp,
        g_gateway_diag_prev_ts_req,
        g_gateway_diag_prev_ts_frame,
        g_gateway_diag_prev_http_req,
        g_gateway_diag_prev_http_conn);

    g_gateway_diag_sample_count.store(i + 1U, std::memory_order_release);

    g_gateway_diag_prev_sample_us = now_us;
    g_gateway_diag_prev_tcp_rx = d.tcp_rx_bytes;
    g_gateway_diag_prev_tcp_tx = d.tcp_tx_bytes;
    g_gateway_diag_prev_usb_rx = d.usb_rx_bytes;
    g_gateway_diag_prev_usb_tx = d.usb_tx_bytes;
    g_gateway_diag_prev_usb_to_tcp = d.usb_to_tcp_bytes;
    g_gateway_diag_prev_ts_req = d.ts_requests;
    g_gateway_diag_prev_ts_frame = d.ts_frames;
    g_gateway_diag_prev_http_req = d.http_requests;
    g_gateway_diag_prev_http_conn = d.http_connections;

    if (i + 1U >= GATEWAY_DIAG_SAMPLE_COUNT) {
        g_gateway_diag_capture_active.store(false, std::memory_order_release);
        g_gateway_diag_capture_ready.store(true, std::memory_order_release);
        g_gateway_diag_next_sample_us = 0;
        return;
    }

    g_gateway_diag_next_sample_us += (uint64_t)GATEWAY_DIAG_SAMPLE_PERIOD_MS * 1000ULL;
    if (g_gateway_diag_next_sample_us <= now_us)
        g_gateway_diag_next_sample_us = now_us + (uint64_t)GATEWAY_DIAG_SAMPLE_PERIOD_MS * 1000ULL;
}

/* ============================================================
   TCP SERVER
   ============================================================ */

static void gateway_tcp_server_task(void *arg)
{
    (void)arg;

    int server_socket = socket(
        AF_INET,
        SOCK_STREAM,
        IPPROTO_IP);

    if (server_socket < 0)
    {
        ESP_LOGE(
            GATEWAY_TAG,
            "TCP socket creation failed: %d",
            errno);
        vTaskDelete(NULL);
        return;
    }

    int reuse = 1;
    (void)setsockopt(
        server_socket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse));

    struct sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("192.168.4.1");
    server_addr.sin_port = htons(GATEWAY_TCP_PORT);

    if (bind(
            server_socket,
            (struct sockaddr *)&server_addr,
            sizeof(server_addr)) < 0)
    {
        ESP_LOGE(
            GATEWAY_TAG,
            "TCP bind failed: %d",
            errno);
        close(server_socket);
        vTaskDelete(NULL);
        return;
    }

    if (listen(
            server_socket,
            GATEWAY_MAX_CLIENTS) < 0)
    {
        ESP_LOGE(
            GATEWAY_TAG,
            "TCP listen failed: %d",
            errno);
        close(server_socket);
        vTaskDelete(NULL);
        return;
    }

    static uint8_t buffer[GATEWAY_TCP_RX_BUFFER_SIZE];

    ESP_LOGI(
        GATEWAY_TAG,
        "TCP listening on 192.168.4.1:%d",
        GATEWAY_TCP_PORT);

    while (true)
    {
        struct sockaddr_in client_addr = {};
        socklen_t client_len = sizeof(client_addr);

        int client = accept(
            server_socket,
            (struct sockaddr *)&client_addr,
            &client_len);

        if (client < 0)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        gateway_reset_session_stats();

        const int old = g_gateway_active_client_socket.exchange(
            client,
            std::memory_order_acq_rel);

        if (old >= 0)
        {
            shutdown(old, SHUT_RDWR);
            close(old);
        }

        int rcvbuf = 256 * 1024;
        int sndbuf = 256 * 1024;

        (void)setsockopt(
            client,
            SOL_SOCKET,
            SO_RCVBUF,
            &rcvbuf,
            sizeof(rcvbuf));

        (void)setsockopt(
            client,
            SOL_SOCKET,
            SO_SNDBUF,
            &sndbuf,
            sizeof(sndbuf));

        int nodelay = 1;
        (void)setsockopt(
            client,
            IPPROTO_TCP,
            TCP_NODELAY,
            &nodelay,
            sizeof(nodelay));

        int tos = 0x80;
        (void)setsockopt(
            client,
            IPPROTO_IP,
            IP_TOS,
            &tos,
            sizeof(tos));

        struct timeval rcv_timeout = {};
        rcv_timeout.tv_sec = 0;
        rcv_timeout.tv_usec = 200000;
        (void)setsockopt(
            client,
            SOL_SOCKET,
            SO_RCVTIMEO,
            &rcv_timeout,
            sizeof(rcv_timeout));

        char client_ip[16] = {};
        inet_ntoa_r(
            client_addr.sin_addr,
            client_ip,
            sizeof(client_ip));

        ESP_LOGI(
            GATEWAY_TAG,
            "TCP client %s:%u",
            client_ip,
            ntohs(client_addr.sin_port));

        if (!gateway_prepare_data_tasks())
        {
            ESP_LOGE(
                GATEWAY_TAG,
                "TCP data path unavailable");

            const int current = g_gateway_active_client_socket.exchange(
                -1,
                std::memory_order_acq_rel);

            if (current >= 0)
            {
                shutdown(current, SHUT_RDWR);
                close(current);
            }

            continue;
        }

        g_gateway_tcp_tx_faults.store(
            0,
            std::memory_order_relaxed);
        g_gateway_tcp_tx_fault_errno.store(
            0,
            std::memory_order_relaxed);

        g_gateway_bridge_active.store(
            true,
            std::memory_order_release);

        vcp_tcp_gateway_set_usb_owner(true);

        if (g_gateway_usb_device_ready.load(std::memory_order_acquire))
        {
            xSemaphoreTake(
                g_gateway_usb_mutex,
                portMAX_DELAY);

            if (g_gateway_usb_device_ready.load(std::memory_order_acquire))
            {
                gateway_setup_usb_baudrate(
                    GATEWAY_USB_BAUDRATE);
            }

            xSemaphoreGive(
                g_gateway_usb_mutex);
        }

        /* Dedicated transport: one TCP task, direct TCP -> USB. */
        while (true)
        {
            gateway_diag_capture_service(gateway_now_us());

            const uint64_t recv_start_us = gateway_now_us();
            const int len = recv(
                client,
                buffer,
                sizeof(buffer),
                0);
            const uint64_t recv_elapsed_us =
                gateway_now_us() - recv_start_us;

            gateway_update_max(
                g_gateway_tcp_recv_time_max_us,
                recv_elapsed_us);

            if (len > 0)
            {
                const uint64_t now_us = gateway_now_us();

                const uint64_t previous_request =
                    g_gateway_ts_request_last_us.exchange(
                        now_us,
                        std::memory_order_relaxed);

                if (previous_request != 0 && now_us >= previous_request)
                {
                    g_gateway_ts_request_interval_us.store(
                        now_us - previous_request,
                        std::memory_order_relaxed);
                }

                g_gateway_ts_requests.fetch_add(
                    1,
                    std::memory_order_relaxed);

                uint64_t zero = 0;
                (void)g_gateway_session_tcp_rx_first_us.compare_exchange_strong(
                    zero,
                    now_us,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed);

                g_gateway_tcp_rx_bytes.fetch_add(
                    (uint64_t)len,
                    std::memory_order_relaxed);
                g_gateway_tcp_rx_chunks.fetch_add(
                    1,
                    std::memory_order_relaxed);

                const uint64_t prev_rx =
                    g_gateway_tcp_last_rx_us.exchange(
                        now_us,
                        std::memory_order_relaxed);

                if (prev_rx != 0 && now_us >= prev_rx)
                {
                    gateway_update_max(
                        g_gateway_tcp_rx_gap_max_us,
                        now_us - prev_rx);
                }

                if (g_gateway_diag_capture_active.load(std::memory_order_acquire))
                {
                    tsdiag_feed(
                        &g_gateway_ts_parser,
                        buffer,
                        (size_t)len,
                        now_us,
                        gateway_ts_frame_event,
                        NULL);

                    g_gateway_ts_parser_in_frame.store(
                        g_gateway_ts_parser.in_frame ? 1 : 0,
                        std::memory_order_relaxed);
                    g_gateway_ts_parser_remaining.store(
                        g_gateway_ts_parser.remaining,
                        std::memory_order_relaxed);
                }

                if (g_gateway_usb_device_ready.load(std::memory_order_acquire))
                {
                    xSemaphoreTake(
                        g_gateway_usb_mutex,
                        portMAX_DELAY);

                    const uint64_t usb_start_us = gateway_now_us();
                    const esp_err_t usb_result =
                        g_gateway_usb_device.send_data(
                            buffer,
                            (size_t)len);
                    const uint64_t usb_elapsed_us =
                        gateway_now_us() - usb_start_us;

                    g_gateway_usb_tx_count.fetch_add(
                        1,
                        std::memory_order_relaxed);

                    if (usb_result == ESP_OK)
                    {
                        g_gateway_usb_tx_bytes.fetch_add(
                            (uint64_t)len,
                            std::memory_order_relaxed);
                    }
                    else
                    {
                        g_gateway_usb_tx_failures.fetch_add(
                            1,
                            std::memory_order_relaxed);
                        g_gateway_usb_tx_last_error.store(
                            (int)usb_result,
                            std::memory_order_relaxed);

                        ESP_LOGW(
                            GATEWAY_TAG,
                            "USB TX failed: 0x%x",
                            usb_result);
                    }

                    g_gateway_usb_tx_time_us.fetch_add(
                        usb_elapsed_us,
                        std::memory_order_relaxed);
                    gateway_update_max(
                        g_gateway_usb_tx_max_us,
                        usb_elapsed_us);

                    xSemaphoreGive(
                        g_gateway_usb_mutex);

                    if (usb_result != ESP_OK)
                        break;
                }

                continue;
            }

            if (len == 0)
            {
                ESP_LOGI(
                    GATEWAY_TAG,
                    "TCP client disconnected");
                break;
            }

            if (errno == EINTR)
            {
                gateway_diag_capture_service(gateway_now_us());
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                gateway_diag_capture_service(gateway_now_us());
                g_gateway_tcp_rx_eagain.fetch_add(
                    1,
                    std::memory_order_relaxed);

                if (g_gateway_tcp_tx_faults.load(
                        std::memory_order_relaxed) != 0)
                {
                    break;
                }

                continue;
            }

            g_gateway_tcp_rx_errors.fetch_add(
                1,
                std::memory_order_relaxed);
            g_gateway_tcp_last_socket_error.store(
                errno,
                std::memory_order_relaxed);

            ESP_LOGW(
                GATEWAY_TAG,
                "TCP recv failed: %d",
                errno);
            break;
        }

        g_gateway_bridge_active.store(
            false,
            std::memory_order_release);
        vcp_tcp_gateway_set_usb_owner(false);
        gateway_print_session_stats();

        const int current = g_gateway_active_client_socket.exchange(
            -1,
            std::memory_order_acq_rel);

        if (current >= 0)
        {
            shutdown(current, SHUT_RDWR);
            close(current);
        }
    }
}


/* ============================================================
   USB CONNECTION MANAGER
   ============================================================ */

static void gateway_connection_manager_task(
    void *arg)
{
    (void)arg;


    while (true)
    {
        /* Keep the USB host connected independently of TCP clients.
           BrickOS HTTP queries cannot wait for lazy USB enumeration. */
        if (!g_gateway_usb_stack_started)
        {
            if (!gateway_start_usb_stack())
            {
                g_brick_usb_stack_failures.fetch_add(1, std::memory_order_relaxed);
                vTaskDelay(pdMS_TO_TICKS(1000) > 0 ? pdMS_TO_TICKS(1000) : 1);
                continue;
            }
        }


        if (
            g_gateway_usb_device_ready.load(
                std::memory_order_acquire))
        {
            vTaskDelay(pdMS_TO_TICKS(50) > 0 ? pdMS_TO_TICKS(50) : 1);

            continue;
        }


        /* The original hub only probed when a TCP client connected. In the
           native client, freeing ALL USB devices on every retry can race USB
           enumeration and perpetually prevent the STM32 CDC port opening.
           Close only our stale handle after an actual disconnect; the USB
           host driver owns discovery and device lifetime otherwise. */
        if (g_gateway_usb_device.need_close)
        {
            if (g_gateway_usb_mutex &&
                xSemaphoreTake(g_gateway_usb_mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
            {
                gateway_close_usb_device();
                xSemaphoreGive(g_gateway_usb_mutex);
            }
            else
            {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20) > 0 ? pdMS_TO_TICKS(20) : 1);


#if GATEWAY_VBUS_CONTROL_GPIO >= 0

        gpio_config_t io_cfg = {};

        io_cfg.intr_type =
            GPIO_INTR_DISABLE;

        io_cfg.mode =
            GPIO_MODE_OUTPUT;

        io_cfg.pin_bit_mask =
            (1ULL <<
             GATEWAY_VBUS_CONTROL_GPIO);

        io_cfg.pull_down_en =
            GPIO_PULLDOWN_DISABLE;

        io_cfg.pull_up_en =
            GPIO_PULLUP_DISABLE;


        (void)gpio_config(
            &io_cfg);


        (void)gpio_set_level(
            (gpio_num_t)
                GATEWAY_VBUS_CONTROL_GPIO,
            0);


        vTaskDelay(
            pdMS_TO_TICKS(150));


        (void)gpio_set_level(
            (gpio_num_t)
                GATEWAY_VBUS_CONTROL_GPIO,
            1);


        vTaskDelay(
            pdMS_TO_TICKS(200));

#endif


        /* Always probe for an ECU, even if no TCP client is connected. */
        if (gateway_open_usb_device())
        {
            ESP_LOGI(
                GATEWAY_TAG,
                "USB device connected");


            gateway_setup_usb_baudrate(
                GATEWAY_USB_BAUDRATE);
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(1000) > 0 ? pdMS_TO_TICKS(1000) : 1);
        }
    }
}


/* ============================================================
   USB HOST / VCP START
   ============================================================ */

static bool gateway_start_usb_stack(void)
{
    if (g_gateway_usb_stack_started)
        return true;


    usb_host_config_t host_cfg = {

        .skip_phy_setup =
            false,

        .intr_flags =
            ESP_INTR_FLAG_LEVEL1
    };


    esp_err_t err =
        usb_host_install(
            &host_cfg);


    if (
        err != ESP_OK &&
        err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(
            GATEWAY_TAG,
            "usb_host_install failed: 0x%x",
            err);

        return false;
    }


    if (
        xTaskCreatePinnedToCore(
            gateway_usb_host_task,
            "gw_usb_host",
            4096,
            NULL,
            GATEWAY_USB_HOST_PRIORITY,
            &g_gateway_usb_host_task_handle,
            GATEWAY_USB_HOST_CORE)
        != pdPASS)
    {
        ESP_LOGE(
            GATEWAY_TAG,
            "USB host task creation failed");

        return false;
    }



    const cdc_acm_host_driver_config_t cdc_cfg = {

        .driver_task_stack_size =
            4096,

        .driver_task_priority =
            GATEWAY_CDC_PRIORITY,

        .xCoreID =
            GATEWAY_USB_HOST_CORE,

        .new_dev_cb =
            NULL
    };


    err =
        cdc_acm_host_install(
            &cdc_cfg);


    if (
        err != ESP_OK &&
        err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(
            GATEWAY_TAG,
            "cdc_acm_host_install failed: 0x%x",
            err);

        return false;
    }


    // CDC is installed FIRST (proven functional in v2.6). Register MSC as
    // a SECOND host client so the same composite STM32 can serve SD LUN1.
    // Failure of optional MSC must never abort or restart the CDC tuner.
    esp_err_t sd_err = brick_sd_ini_host_install();
    if (sd_err != ESP_OK) {
        ESP_LOGW(GATEWAY_TAG, "Optional SD reader unavailable: %s", esp_err_to_name(sd_err));
    }

    // STM32 CDC-ACM uses the Espressif CDC host driver directly.


    g_gateway_usb_stack_started =
        true;


    ESP_LOGI(
        GATEWAY_TAG,
        "USB host/VCP infrastructure active");


    return true;
}


/* ============================================================
   DATA PATH SETUP
   ============================================================ */

static bool gateway_prepare_data_tasks(void)
{
    if (g_gateway_data_tasks_started)
        return true;

    if (g_gateway_device_disconnected_sem == NULL)
        g_gateway_device_disconnected_sem = xSemaphoreCreateBinary();

    if (g_gateway_usb_mutex == NULL)
        g_gateway_usb_mutex = xSemaphoreCreateMutex();

    if (!g_gateway_device_disconnected_sem || !g_gateway_usb_mutex)
        return false;

    /* No TCP worker or stream buffer is created in the dedicated path. */
    g_gateway_data_tasks_started = true;
    return true;
}

/* ============================================================
   STARTUP
   ============================================================ */

int vcp_tcp_gateway_start(void)
{
    if (g_gateway_start_mutex == NULL)
    {
        g_gateway_start_mutex =
            xSemaphoreCreateMutex();
    }


    if (g_gateway_start_mutex == NULL)
        return ESP_ERR_NO_MEM;


    xSemaphoreTake(
        g_gateway_start_mutex,
        portMAX_DELAY);


    if (g_gateway_started)
    {
        xSemaphoreGive(
            g_gateway_start_mutex);

        return ESP_OK;
    }


    if (g_ap_netif == NULL)
    {
        ESP_LOGE(
            GATEWAY_TAG,
            "Hub AP netif is not initialized");


        xSemaphoreGive(
            g_gateway_start_mutex);


        return ESP_ERR_INVALID_STATE;
    }


    /* Initialize synchronization before the USB manager can open the ECU. */
    if (!gateway_prepare_data_tasks())
    {
        xSemaphoreGive(g_gateway_start_mutex);
        return ESP_ERR_NO_MEM;
    }

    if (!g_brick_usb_rx_stream)
        g_brick_usb_rx_stream = xStreamBufferCreate(8192, 1);
    if (!g_brick_usb_rx_stream)
    {
        xSemaphoreGive(g_gateway_start_mutex);
        return ESP_ERR_NO_MEM;
    }


    if (
        xTaskCreatePinnedToCore(
            gateway_connection_manager_task,
            "gw_usb_mgr",
            4096,
            NULL,
            4, // USB connect manager is background work; keep CPU0 idle/Wi-Fi running.
            &g_gateway_usb_mgr_task_handle,
            GATEWAY_USB_HOST_CORE)
        != pdPASS)
    {
        xSemaphoreGive(
            g_gateway_start_mutex);

        return ESP_ERR_NO_MEM;
    }


    /* No TCP server: BrickOS is the TunerStudio client itself. The USB
       manager starts independently and the web UI sends native requests. */

    g_gateway_started =
        true;


    ESP_LOGI(
        GATEWAY_TAG,
        "BrickOS native USB TunerStudio client; USB host auto-connect active");


    xSemaphoreGive(
        g_gateway_start_mutex);


    return ESP_OK;
}


/* ============================================================
   ON-DEVICE 15-SECOND DIAGNOSTIC CAPTURE API
   ============================================================ */

extern "C" bool vcp_gateway_ts_diag_capture_start(void)
{
    bool expected = false;
    if (!g_gateway_diag_capture_active.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        g_gateway_diag_capture_error.store(1, std::memory_order_release);
        return false;
    }

    g_gateway_diag_capture_error.store(0, std::memory_order_release);
    gateway_diag_capture_reset();

    g_gateway_diag_next_sample_us = g_gateway_diag_capture_start_us.load(std::memory_order_relaxed);
    g_gateway_diag_prev_tcp_rx = 0;
    g_gateway_diag_prev_tcp_tx = 0;
    g_gateway_diag_prev_usb_rx = 0;
    g_gateway_diag_prev_usb_tx = 0;
    g_gateway_diag_prev_usb_to_tcp = 0;
    g_gateway_diag_prev_ts_req = 0;
    g_gateway_diag_prev_ts_frame = 0;
    g_gateway_diag_prev_http_req = g_gateway_diag_http_req_base;
    g_gateway_diag_prev_http_conn = g_gateway_diag_http_conn_base;
    g_gateway_diag_prev_sample_us = 0;

    /* No heap allocation.  No new FreeRTOS task.  The existing TCP server
     * task services capture sampling while the TunerStudio connection is live. */
    g_gateway_diag_sample_count.store(0, std::memory_order_release);
    g_gateway_diag_capture_ready.store(false, std::memory_order_release);
    return true;
}

extern "C" bool vcp_gateway_ts_diag_capture_active(void)
{
    return g_gateway_diag_capture_active.load(std::memory_order_acquire);
}

extern "C" bool vcp_gateway_ts_diag_capture_ready(void)
{
    return g_gateway_diag_capture_ready.load(std::memory_order_acquire);
}

extern "C" int32_t vcp_gateway_ts_diag_capture_error(void)
{
    return g_gateway_diag_capture_error.load(std::memory_order_acquire);
}

static uint32_t gateway_diag_rate(uint64_t now_value, uint64_t prev_value, uint64_t now_us, uint64_t prev_us)
{
    if (now_us <= prev_us) return 0;
    const uint64_t rate = ((now_value - prev_value) * 1000000ULL) / (now_us - prev_us);
    return (uint32_t)(rate > UINT32_MAX ? UINT32_MAX : rate);
}



static int gateway_diag_format_row(char *out, size_t out_cap,
                                   uint32_t i,
                                   const GatewayDiagSample &d,
                                   const GatewayDiagSample *prev)
{
    const uint32_t tcp_rx_hz = prev ? gateway_diag_rate(d.tcp_rx_chunks, prev->tcp_rx_chunks, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
    const uint32_t tcp_rx_bps = prev ? gateway_diag_rate(d.tcp_rx_bytes, prev->tcp_rx_bytes, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
    const uint32_t tcp_tx_hz = prev ? gateway_diag_rate(d.tcp_tx_calls, prev->tcp_tx_calls, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
    const uint32_t tcp_tx_bps = prev ? gateway_diag_rate(d.tcp_tx_bytes, prev->tcp_tx_bytes, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
    const uint32_t usb_rx_hz = prev ? gateway_diag_rate(d.usb_rx_chunks, prev->usb_rx_chunks, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
    const uint32_t usb_rx_bps = prev ? gateway_diag_rate(d.usb_rx_bytes, prev->usb_rx_bytes, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
    const uint32_t usb_tx_hz = prev ? gateway_diag_rate(d.usb_tx_count, prev->usb_tx_count, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
    const uint32_t usb_tx_bps = prev ? gateway_diag_rate(d.usb_tx_bytes, prev->usb_tx_bytes, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
    const uint32_t usb_to_tcp_hz = prev ? gateway_diag_rate(d.usb_to_tcp_chunks, prev->usb_to_tcp_chunks, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
    const uint32_t usb_to_tcp_bps = prev ? gateway_diag_rate(d.usb_to_tcp_bytes, prev->usb_to_tcp_bytes, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
    const uint32_t ts_req_hz = prev ? gateway_diag_rate(d.ts_requests, prev->ts_requests, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
    const uint32_t ts_frame_hz = prev ? gateway_diag_rate(d.ts_frames, prev->ts_frames, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;

    (void)i;
    return snprintf(out, out_cap,
            "%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIi32 "|"
            "%" PRIi32 "|%" PRIi32 "|%" PRIi32 "|%" PRIi32 "|%" PRIi32 "|%" PRIi32 "|%" PRIi32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|"
            "%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|"
            "%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIi32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|"
            "%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|"
            "%" PRIu32 "|%" PRIi32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|"
            "%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIi32 "|"
            "%" PRIi32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|"
            "%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "",
            d.rel_ms,d.uptime_s,d.active,d.usb_ready,d.cpu0_load_pct,d.cpu1_load_pct,d.free_heap,d.largest_heap,d.min_free_heap,d.sta_count,d.sta_rssi,
            d.tcp_fd,d.tcp_so_error,d.tcp_rcvbuf,d.tcp_sndbuf,d.tcp_nodelay,d.tcp_pending,d.tcp_rx_bytes,d.tcp_rx_chunks,tcp_rx_hz,tcp_rx_bps,(uint32_t)(d.tcp_rx_gap_max_us/1000ULL),d.tcp_recv_time_max_us,d.tcp_rx_eagain,d.tcp_rx_errors,
            d.tcp_tx_bytes,d.tcp_tx_calls,tcp_tx_hz,tcp_tx_bps,d.tcp_tx_avg_us,d.tcp_tx_max_us,(uint32_t)(d.tcp_tx_gap_max_us/1000ULL),d.tcp_tx_eagain,d.tcp_tx_partial,d.tcp_tx_faults,d.tcp_tx_fault_errno,(uint32_t)(d.tcp_last_rx_age_us/1000ULL),(uint32_t)(d.tcp_last_tx_age_us/1000ULL),
            d.usb_rx_bytes,d.usb_rx_chunks,usb_rx_hz,usb_rx_bps,(uint32_t)(d.usb_rx_interval_us/1000ULL),(uint32_t)(d.usb_rx_interval_max_us/1000ULL),d.usb_tx_bytes,d.usb_tx_count,usb_tx_hz,usb_tx_bps,d.usb_tx_avg_us,d.usb_tx_max_us,d.usb_tx_failures,d.usb_tx_last_error,
            d.usb_to_tcp_bytes,d.usb_to_tcp_chunks,usb_to_tcp_hz,usb_to_tcp_bps,d.usb_to_tcp_drop_bytes,d.usb_to_tcp_queue_bytes,d.usb_to_tcp_queue_delay_us,d.usb_to_tcp_queue_delay_max_us,
            d.ts_requests,ts_req_hz,(uint32_t)(d.ts_request_interval_us/1000ULL),d.ts_frames,ts_frame_hz,(uint32_t)(d.ts_frame_interval_us/1000ULL),(uint32_t)(d.ts_frame_interval_min_us/1000ULL),(uint32_t)(d.ts_frame_interval_max_us/1000ULL),d.ts_legacy,d.ts_last_command,d.ts_parser_in_frame,d.ts_parser_remaining,d.ts_frame_to_usb_rx_us,d.ts_frame_to_usb_rx_max_us,(uint32_t)(d.ts_roundtrip_us/1000ULL),(uint32_t)(d.ts_roundtrip_max_us/1000ULL),
            d.http_requests,d.http_connections,d.http_req_rate_hz,d.http_conn_rate_hz,d.gw_hwm_words,d.usb_mgr_hwm_words,d.tcp_tx_hwm_words,d.usb_host_hwm_words,d.http_hwm_words,d.json_hwm_words);
}

static bool gateway_http_send_all(int fd, const char *data, size_t len)
{
    size_t sent_total = 0;
    while (sent_total < len) {
        const int n = send(fd, data + sent_total, len - sent_total, 0);
        if (n <= 0) return false;
        sent_total += (size_t)n;
    }
    return true;
}

static bool gateway_http_send_chunk(int fd, const char *data, size_t len)
{
    char head[24];
    const int h = snprintf(head, sizeof(head), "%zx\r\n", len);
    if (h <= 0 || (size_t)h >= sizeof(head)) return false;
    if (!gateway_http_send_all(fd, head, (size_t)h)) return false;
    if (len && !gateway_http_send_all(fd, data, len)) return false;
    return gateway_http_send_all(fd, "\r\n", 2);
}

extern "C" bool vcp_gateway_ts_diag_send_log(int fd)
{
    if (fd < 0 ||
        !gateway_diag_storage_allocated() ||
        !g_gateway_diag_capture_ready.load(std::memory_order_acquire))
        return false;

    /*
     * Send this as a normal HTTP response with Content-Length instead of
     * chunked transfer encoding.  Safari/iOS can leave a fetch() of a
     * chunked, progressively-generated response pending until the connection
     * closes, which makes the phone UI look like it is stuck on "fetching log".
     *
     * The capture is only 151 samples, so doing a formatting pass first is
     * inexpensive and lets us give the browser the exact response length.
     */
    static const char header[] =
        "REL_MS|UPTIME_S|ACTIVE|USB_READY|CPU0_LOAD_PCT|CPU1_LOAD_PCT|FREE_HEAP|LARGEST_HEAP|MIN_FREE_HEAP|STA_COUNT|STA_RSSI|"
        "TCP_FD|TCP_SO_ERROR|TCP_RCVBUF|TCP_SNDBUF|TCP_NODELAY|TCP_PENDING|TCP_RX_BYTES|TCP_RX_CHUNKS|TCP_RX_HZ|TCP_RX_BPS|TCP_RX_GAP_MAX_MS|TCP_RECV_MAX_US|TCP_RX_EAGAIN|TCP_RX_ERRORS|"
        "TCP_TX_BYTES|TCP_TX_CALLS|TCP_TX_HZ|TCP_TX_BPS|TCP_TX_AVG_US|TCP_TX_MAX_US|TCP_TX_GAP_MAX_MS|TCP_TX_EAGAIN|TCP_TX_PARTIAL|TCP_TX_FAULTS|TCP_TX_ERRNO|TCP_LAST_RX_AGE_MS|TCP_LAST_TX_AGE_MS|"
        "USB_RX_BYTES|USB_RX_CHUNKS|USB_RX_HZ|USB_RX_BPS|USB_RX_INTERVAL_MS|USB_RX_MAX_MS|USB_TX_BYTES|USB_TX_COUNT|USB_TX_HZ|USB_TX_BPS|USB_TX_AVG_US|USB_TX_MAX_US|USB_TX_FAILS|USB_TX_ERR|"
        "USB_TO_TCP_BYTES|USB_TO_TCP_CHUNKS|USB_TO_TCP_HZ|USB_TO_TCP_BPS|USB_TO_TCP_DROPS|USB_Q_BYTES|USB_LAST_ENQ_AGE_US|USB_LAST_ENQ_AGE_MAX_US|"
        "TS_REQUESTS|TS_REQ_HZ|TS_REQ_INTERVAL_MS|TS_FRAMES|TS_FRAME_HZ|TS_FRAME_INTERVAL_MS|TS_FRAME_MIN_MS|TS_FRAME_MAX_MS|TS_LEGACY|TS_LAST_CMD|TS_PARSER_IN_FRAME|TS_PARSER_REMAINING|TS_FRAME_USB_LAT_US|TS_FRAME_USB_LAT_MAX_US|TS_RTT_MS|TS_RTT_MAX_MS|"
        "HTTP_REQUESTS|HTTP_CONNECTIONS|HTTP_REQ_HZ|HTTP_CONN_HZ|GW_HWM_WORDS|USB_MGR_HWM_WORDS|TCP_TX_HWM_WORDS|USB_HOST_HWM_WORDS|HTTP_HWM_WORDS|JSON_HWM_WORDS\n";

    const uint32_t count =
        g_gateway_diag_sample_count.load(std::memory_order_acquire);

    char row[2048];
    size_t body_len = sizeof(header) - 1U;

    /* First pass: calculate the exact body length. */
    for (uint32_t i = 0; i < count; ++i)
    {
        const GatewayDiagSample *dp = gateway_diag_sample_ptr(i);
        if (!dp)
            return false;

        const GatewayDiagSample *prev =
            (i > 0) ? gateway_diag_sample_ptr(i - 1) : nullptr;

        const int n =
            gateway_diag_format_row(row, sizeof(row), i, *dp, prev);

        if (n < 0 || (size_t)n >= sizeof(row))
            return false;

        body_len += (size_t)n + 1U; /* row + newline */
    }

    char http_header[384];
    const int hn = snprintf(
        http_header,
        sizeof(http_header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: %lu\r\n"
        "Cache-Control: no-store, no-cache, must-revalidate\r\n"
        "Pragma: no-cache\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        (unsigned long)body_len);

    if (hn <= 0 || (size_t)hn >= sizeof(http_header))
        return false;

    if (!gateway_http_send_all(fd, http_header, (size_t)hn))
        return false;

    if (!gateway_http_send_all(fd, header, sizeof(header) - 1U))
        return false;

    /* Second pass: send the exact body whose length was advertised. */
    for (uint32_t i = 0; i < count; ++i)
    {
        const GatewayDiagSample *dp = gateway_diag_sample_ptr(i);
        if (!dp)
            return false;

        const GatewayDiagSample *prev =
            (i > 0) ? gateway_diag_sample_ptr(i - 1) : nullptr;

        const int n =
            gateway_diag_format_row(row, sizeof(row), i, *dp, prev);

        if (n < 0 || (size_t)n >= sizeof(row))
            return false;

        if (!gateway_http_send_all(fd, row, (size_t)n))
            return false;

        if (!gateway_http_send_all(fd, "\n", 1))
            return false;
    }

    return true;
}

extern "C" size_t vcp_gateway_ts_diag_log(char *out, size_t out_cap)
{
    if (!out || out_cap < 2 || !gateway_diag_storage_allocated() || !g_gateway_diag_capture_ready.load(std::memory_order_acquire))
        return 0;
    const uint32_t count = g_gateway_diag_sample_count.load(std::memory_order_acquire);
    size_t used = 0;
    const char *header =
        "REL_MS|UPTIME_S|ACTIVE|USB_READY|CPU0_LOAD_PCT|CPU1_LOAD_PCT|FREE_HEAP|LARGEST_HEAP|MIN_FREE_HEAP|STA_COUNT|STA_RSSI|"
        "TCP_FD|TCP_SO_ERROR|TCP_RCVBUF|TCP_SNDBUF|TCP_NODELAY|TCP_PENDING|TCP_RX_BYTES|TCP_RX_CHUNKS|TCP_RX_HZ|TCP_RX_BPS|TCP_RX_GAP_MAX_MS|TCP_RECV_MAX_US|TCP_RX_EAGAIN|TCP_RX_ERRORS|"
        "TCP_TX_BYTES|TCP_TX_CALLS|TCP_TX_HZ|TCP_TX_BPS|TCP_TX_AVG_US|TCP_TX_MAX_US|TCP_TX_GAP_MAX_MS|TCP_TX_EAGAIN|TCP_TX_PARTIAL|TCP_TX_FAULTS|TCP_TX_ERRNO|TCP_LAST_RX_AGE_MS|TCP_LAST_TX_AGE_MS|"
        "USB_RX_BYTES|USB_RX_CHUNKS|USB_RX_HZ|USB_RX_BPS|USB_RX_INTERVAL_MS|USB_RX_MAX_MS|USB_TX_BYTES|USB_TX_COUNT|USB_TX_HZ|USB_TX_BPS|USB_TX_AVG_US|USB_TX_MAX_US|USB_TX_FAILS|USB_TX_ERR|"
        "USB_TO_TCP_BYTES|USB_TO_TCP_CHUNKS|USB_TO_TCP_HZ|USB_TO_TCP_BPS|USB_TO_TCP_DROPS|USB_Q_BYTES|USB_LAST_ENQ_AGE_US|USB_LAST_ENQ_AGE_MAX_US|"
        "TS_REQUESTS|TS_REQ_HZ|TS_REQ_INTERVAL_MS|TS_FRAMES|TS_FRAME_HZ|TS_FRAME_INTERVAL_MS|TS_FRAME_MIN_MS|TS_FRAME_MAX_MS|TS_LEGACY|TS_LAST_CMD|TS_PARSER_IN_FRAME|TS_PARSER_REMAINING|TS_FRAME_USB_LAT_US|TS_FRAME_USB_LAT_MAX_US|TS_RTT_MS|TS_RTT_MAX_MS|"
        "HTTP_REQUESTS|HTTP_CONNECTIONS|HTTP_REQ_HZ|HTTP_CONN_HZ|GW_HWM_WORDS|USB_MGR_HWM_WORDS|TCP_TX_HWM_WORDS|USB_HOST_HWM_WORDS|HTTP_HWM_WORDS|JSON_HWM_WORDS\n";
    const size_t hlen = strlen(header);
    if (hlen >= out_cap) return 0;
    memcpy(out, header, hlen); used = hlen;
    for (uint32_t i=0; i<count; ++i) {
        const GatewayDiagSample *dp = gateway_diag_sample_ptr(i);
            if (!dp) break;
        const GatewayDiagSample &d = *dp;
        const GatewayDiagSample *prev = (i > 0) ? gateway_diag_sample_ptr(i-1) : NULL;
        const uint64_t dt_us = prev && d.rel_ms >= prev->rel_ms ? (uint64_t)(d.rel_ms - prev->rel_ms) * 1000ULL : 100000ULL;
        const uint32_t tcp_rx_hz = prev ? gateway_diag_rate(d.tcp_rx_chunks, prev->tcp_rx_chunks, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
        const uint32_t tcp_rx_bps = prev ? gateway_diag_rate(d.tcp_rx_bytes, prev->tcp_rx_bytes, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
        const uint32_t tcp_tx_hz = prev ? gateway_diag_rate(d.tcp_tx_calls, prev->tcp_tx_calls, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
        const uint32_t tcp_tx_bps = prev ? gateway_diag_rate(d.tcp_tx_bytes, prev->tcp_tx_bytes, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
        const uint32_t usb_rx_hz = prev ? gateway_diag_rate(d.usb_rx_chunks, prev->usb_rx_chunks, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
        const uint32_t usb_rx_bps = prev ? gateway_diag_rate(d.usb_rx_bytes, prev->usb_rx_bytes, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
        const uint32_t usb_tx_hz = prev ? gateway_diag_rate(d.usb_tx_count, prev->usb_tx_count, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
        const uint32_t usb_tx_bps = prev ? gateway_diag_rate(d.usb_tx_bytes, prev->usb_tx_bytes, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
        const uint32_t usb_to_tcp_hz = prev ? gateway_diag_rate(d.usb_to_tcp_chunks, prev->usb_to_tcp_chunks, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
        const uint32_t usb_to_tcp_bps = prev ? gateway_diag_rate(d.usb_to_tcp_bytes, prev->usb_to_tcp_bytes, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
        const uint32_t ts_req_hz = prev ? gateway_diag_rate(d.ts_requests, prev->ts_requests, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
        const uint32_t ts_frame_hz = prev ? gateway_diag_rate(d.ts_frames, prev->ts_frames, (uint64_t)d.rel_ms*1000ULL, (uint64_t)prev->rel_ms*1000ULL) : 0;
        const int n = snprintf(out + used, out_cap - used,
            "%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIi32 "|"
            "%" PRIi32 "|%" PRIi32 "|%" PRIi32 "|%" PRIi32 "|%" PRIi32 "|%" PRIi32 "|%" PRIi32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|"
            "%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|"
            "%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIi32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|"
            "%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|"
            "%" PRIu32 "|%" PRIi32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|"
            "%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIi32 "|"
            "%" PRIi32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|"
            "%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "|%" PRIu32 "",
            d.rel_ms,d.uptime_s,d.active,d.usb_ready,d.cpu0_load_pct,d.cpu1_load_pct,d.free_heap,d.largest_heap,d.min_free_heap,d.sta_count,d.sta_rssi,
            d.tcp_fd,d.tcp_so_error,d.tcp_rcvbuf,d.tcp_sndbuf,d.tcp_nodelay,d.tcp_pending,d.tcp_rx_bytes,d.tcp_rx_chunks,tcp_rx_hz,tcp_rx_bps,(uint32_t)(d.tcp_rx_gap_max_us/1000ULL),d.tcp_recv_time_max_us,d.tcp_rx_eagain,d.tcp_rx_errors,
            d.tcp_tx_bytes,d.tcp_tx_calls,tcp_tx_hz,tcp_tx_bps,d.tcp_tx_avg_us,d.tcp_tx_max_us,(uint32_t)(d.tcp_tx_gap_max_us/1000ULL),d.tcp_tx_eagain,d.tcp_tx_partial,d.tcp_tx_faults,d.tcp_tx_fault_errno,(uint32_t)(d.tcp_last_rx_age_us/1000ULL),(uint32_t)(d.tcp_last_tx_age_us/1000ULL),
            d.usb_rx_bytes,d.usb_rx_chunks,usb_rx_hz,usb_rx_bps,(uint32_t)(d.usb_rx_interval_us/1000ULL),(uint32_t)(d.usb_rx_interval_max_us/1000ULL),d.usb_tx_bytes,d.usb_tx_count,usb_tx_hz,usb_tx_bps,d.usb_tx_avg_us,d.usb_tx_max_us,d.usb_tx_failures,d.usb_tx_last_error,
            d.usb_to_tcp_bytes,d.usb_to_tcp_chunks,usb_to_tcp_hz,usb_to_tcp_bps,d.usb_to_tcp_drop_bytes,d.usb_to_tcp_queue_bytes,d.usb_to_tcp_queue_delay_us,d.usb_to_tcp_queue_delay_max_us,
            d.ts_requests,ts_req_hz,(uint32_t)(d.ts_request_interval_us/1000ULL),d.ts_frames,ts_frame_hz,(uint32_t)(d.ts_frame_interval_us/1000ULL),(uint32_t)(d.ts_frame_interval_min_us/1000ULL),(uint32_t)(d.ts_frame_interval_max_us/1000ULL),d.ts_legacy,d.ts_last_command,d.ts_parser_in_frame,d.ts_parser_remaining,d.ts_frame_to_usb_rx_us,d.ts_frame_to_usb_rx_max_us,(uint32_t)(d.ts_roundtrip_us/1000ULL),(uint32_t)(d.ts_roundtrip_max_us/1000ULL),
            d.http_requests,d.http_connections,d.http_req_rate_hz,d.http_conn_rate_hz,d.gw_hwm_words,d.usb_mgr_hwm_words,d.tcp_tx_hwm_words,d.usb_host_hwm_words,d.http_hwm_words,d.json_hwm_words);
        if (n < 0 || (size_t)n >= out_cap - used) return used;
        used += (size_t)n;
        (void)dt_us;
    }
    return used;
}


extern "C" void vcp_gateway_ts_diag_capture_release(void)
{
    if (gateway_diag_storage_allocated() &&
        !g_gateway_diag_capture_active.load(std::memory_order_acquire)) {
        gateway_diag_storage_free();
        g_gateway_diag_sample_count.store(0, std::memory_order_release);
        g_gateway_diag_capture_ready.store(false, std::memory_order_release);
        g_gateway_diag_capture_error.store(0, std::memory_order_release);
        g_gateway_diag_next_sample_us = 0;
    }
}


/* ============================================================
   WEB DIAGNOSTIC GETTERS
   ============================================================ */

extern "C"
uint32_t vcp_gateway_ts_requests(void)
{
    return (uint32_t)
        g_gateway_ts_requests.load(
            std::memory_order_relaxed);
}


extern "C"
uint32_t vcp_gateway_ts_request_rate(void)
{
    const uint64_t interval_us =
        g_gateway_ts_request_interval_us.load(
            std::memory_order_relaxed);

    if (interval_us == 0)
        return 0;

    return (uint32_t)(
        1000000ULL / interval_us);
}


extern "C"
uint32_t vcp_gateway_ts_request_interval_ms(void)
{
    return (uint32_t)(
        g_gateway_ts_request_interval_us.load(
            std::memory_order_relaxed) /
        1000ULL);
}


extern "C"
uint32_t vcp_gateway_ts_usb_tx_count(void)
{
    return (uint32_t)
        g_gateway_usb_tx_count.load(
            std::memory_order_relaxed);
}


extern "C"
uint32_t vcp_gateway_ts_usb_tx_avg_us(void)
{
    const uint64_t count =
        g_gateway_usb_tx_count.load(
            std::memory_order_relaxed);

    if (count == 0)
        return 0;

    return (uint32_t)(
        g_gateway_usb_tx_time_us.load(
            std::memory_order_relaxed) /
        count);
}


extern "C"
uint32_t vcp_gateway_ts_usb_tx_max_us(void)
{
    return (uint32_t)
        g_gateway_usb_tx_max_us.load(
            std::memory_order_relaxed);
}


extern "C"
uint32_t vcp_gateway_ts_usb_rx_count(void)
{
    return (uint32_t)
        g_gateway_ts_usb_rx_count.load(
            std::memory_order_relaxed);
}


extern "C"
uint32_t vcp_gateway_ts_usb_rx_interval_ms(void)
{
    return (uint32_t)(
        g_gateway_ts_usb_rx_interval_us.load(
            std::memory_order_relaxed) /
        1000ULL);
}


extern "C"
bool vcp_gateway_ts_tcp_active(void)
{
    return g_gateway_bridge_active.load(
        std::memory_order_acquire);
}


extern "C"
uint32_t vcp_gateway_ts_session_uptime_s(void)
{
    const uint64_t start_us =
        g_gateway_session_start_us.load(
            std::memory_order_relaxed);

    if (start_us == 0)
        return 0;

    const uint64_t now_us =
        gateway_now_us();

    if (now_us < start_us)
        return 0;

    return (uint32_t)((now_us - start_us) / 1000000ULL);
}


extern "C"
uint32_t vcp_gateway_ts_tcp_tx_avg_us(void)
{
    const uint64_t count =
        g_gateway_tcp_tx_count.load(
            std::memory_order_relaxed);

    if (count == 0)
        return 0;

    return (uint32_t)(
        g_gateway_tcp_tx_time_us.load(
            std::memory_order_relaxed) /
        count);
}


extern "C"
uint32_t vcp_gateway_ts_tcp_tx_max_us(void)
{
    return (uint32_t)
        g_gateway_tcp_tx_max_us.load(
            std::memory_order_relaxed);
}


extern "C"
uint32_t vcp_gateway_ts_usb_to_tcp_drop_bytes(void)
{
    return (uint32_t)g_gateway_usb_to_tcp_drop_bytes.load(
        std::memory_order_relaxed);
}

extern "C"
uint32_t vcp_gateway_ts_tcp_tx_faults(void)
{
    return (uint32_t)g_gateway_tcp_tx_faults.load(
        std::memory_order_relaxed);
}

extern "C"
int32_t vcp_gateway_ts_tcp_tx_fault_errno(void)
{
    return (int32_t)g_gateway_tcp_tx_fault_errno.load(
        std::memory_order_relaxed);
}


extern "C"
uint32_t vcp_gateway_ts_roundtrip_ms(void)
{
    return (uint32_t)(
        g_gateway_ts_roundtrip_us.load(
            std::memory_order_relaxed) /
        1000ULL);
}


extern "C"
uint32_t vcp_gateway_ts_roundtrip_max_ms(void)
{
    return (uint32_t)(
        g_gateway_ts_roundtrip_max_us.load(
            std::memory_order_relaxed) /
        1000ULL);
}
extern "C" bool vcp_gateway_usb_ready(void)
{
    return g_gateway_usb_device_ready.load(std::memory_order_acquire);
}

/* Native, single-owner USB transaction interface for protocol.c. */
extern "C" esp_err_t brick_usb_transaction_begin(void)
{
    if (!g_gateway_usb_device_ready.load(std::memory_order_acquire))
        return ESP_ERR_NOT_FOUND;
    if (!g_brick_usb_rx_stream || !g_gateway_usb_mutex)
        return ESP_ERR_INVALID_STATE;
    bool expected = false;
    if (!g_brick_usb_transaction.compare_exchange_strong(
            expected, true, std::memory_order_acq_rel))
        return ESP_ERR_INVALID_STATE;
    xStreamBufferReset(g_brick_usb_rx_stream);
    return ESP_OK;
}

extern "C" esp_err_t brick_usb_transaction_write(const uint8_t *data, size_t len)
{
    if (!data || !len || !g_brick_usb_transaction.load(std::memory_order_acquire))
        return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(g_gateway_usb_mutex, pdMS_TO_TICKS(2000)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    esp_err_t result = ESP_ERR_NOT_FOUND;
    if (g_gateway_usb_device_ready.load(std::memory_order_acquire))
        result = g_gateway_usb_device.send_data(data, len);
    xSemaphoreGive(g_gateway_usb_mutex);
    return result;
}

extern "C" size_t brick_usb_transaction_read(uint8_t *dst, size_t len, uint32_t timeout_ms)
{
    if (!dst || !len || !g_brick_usb_transaction.load(std::memory_order_acquire)
        || !g_brick_usb_rx_stream)
        return 0;
    TickType_t wait = pdMS_TO_TICKS(timeout_ms);
    if (timeout_ms && wait == 0) wait = 1;
    return xStreamBufferReceive(g_brick_usb_rx_stream, dst, len, wait);
}

extern "C" void brick_usb_transaction_end(void)
{
    g_brick_usb_transaction.store(false, std::memory_order_release);
}

extern "C" uint32_t brick_usb_open_attempts(void) { return g_brick_usb_open_attempts.load(std::memory_order_relaxed); }
extern "C" uint32_t brick_usb_open_failures(void) { return g_brick_usb_open_failures.load(std::memory_order_relaxed); }
extern "C" uint32_t brick_usb_disconnects(void) { return g_brick_usb_disconnects.load(std::memory_order_relaxed); }
extern "C" uint32_t brick_usb_stack_failures(void) { return g_brick_usb_stack_failures.load(std::memory_order_relaxed); }
extern "C" uint32_t brick_usb_successful_opens(void) { return g_brick_usb_successful_opens.load(std::memory_order_relaxed); }
extern "C" bool brick_usb_stack_started(void) { return g_gateway_usb_stack_started; }
