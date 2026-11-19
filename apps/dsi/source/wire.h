#ifndef BTS_DSI_WIRE_H
#define BTS_DSI_WIRE_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "tuner.h"
#ifndef BTS_ESP_HOST
#define BTS_ESP_HOST "192.168.4.1"
#endif
#ifndef BTS_ESP_PORT
#define BTS_ESP_PORT 80
#endif

typedef void (*BtProgressFn)(size_t total_bytes);
void bt_set_progress(BtProgressFn callback);
typedef bool (*BtChunkFn)(const uint8_t *data, size_t n, void *user);
typedef struct {int code;size_t received;bool complete;} BtHttpResult;
BtHttpResult bt_http(const char *method,const char *path,const char *post_data,BtChunkFn consumer,void *user,size_t maxbytes);
bool bt_get_status(char *json,size_t size);
bool bt_get_ini(BtDefinition *d);
bool bt_get_live(BtLive *live,int length);
bool bt_read_raw(const BtDefinition*d,const BtField*f,uint8_t*dst,int cap);
bool bt_commit_field(const BtDefinition*d,const BtField*f,const uint8_t*old_data,
                     const uint8_t*new_data,int length,char*reason,size_t reason_cap);
bool bt_read_field(const BtDefinition*d,const BtField*f,double *values,int capacity);
bool bt_json_bool(const char*json,const char*key,bool *out);
bool bt_json_string(const char*json,const char*key,char*out,size_t capacity);
/* Cooperative, nonblocking GET transport. Call bt_async_poll() once per frame.
 * No network operation may hold up input scanning or display updates. */
typedef enum {BT_NET_NONE, BT_NET_STATUS, BT_NET_INI, BT_NET_LIVE, BT_NET_READ} BtNetJob;
bool bt_async_begin(BtNetJob job, BtDefinition *definition, BtLive *live, int live_length, int frame);
/* Returns 0 while pending, +1 on a complete verified response, -1 on error. */
/* Cooperative calibration R reads, through the same persistent HTTP socket.
 * One request reads at most 256 bytes. The caller owns page/offset bounds,
 * reads the response ONLY after poll returns +1, and remains read-only. */
bool bt_async_begin_cal_read(int page_id,int offset,int count,int frame);
bool bt_async_cal_read_result(uint8_t *dst,int count);
int bt_async_poll(int frame);
void bt_async_cancel(void);
void bt_async_forget_connection(void);
void bt_async_expected_signature(const char *signature);
bool bt_cache_begin(BtDefinition *definition);
int bt_cache_poll(void);
size_t bt_cache_bytes(void);
bool bt_cache_out_of_memory(void);
void bt_cache_cancel(void);
bool bt_async_busy(void);
BtNetJob bt_async_job(void);
const char *bt_async_phase(void);
const char *bt_async_error(void);
const char *bt_async_request_path(void);
const char *bt_async_status_line(void);
const char *bt_async_reject_body(void);
const char *bt_async_status_json(void);
size_t bt_async_bytes(void);
size_t bt_async_saved_bytes(void);
size_t bt_async_parsed_bytes(void);
size_t bt_async_total(void);
int bt_async_http_status(void);
bool bt_async_live_is_new(void);
bool bt_async_signature_mismatch(void);

#endif /* BTS_DSI_WIRE_H */
