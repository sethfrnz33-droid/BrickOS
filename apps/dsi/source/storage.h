#ifndef BTS_DSI_STORAGE_H
#define BTS_DSI_STORAGE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "tuner.h"

/* The raw ECU INI lives on SD, never in a multi-megabyte network buffer. */
bool bt_sd_init(void);
bool bt_sd_ready(void);
const char *bt_sd_asset_root(void);
int bt_sd_layout_load(void);
bool bt_sd_layout_save(int layout);
const char *bt_sd_message(void);
size_t bt_sd_written(void);
const char *bt_sd_cache_path(void);
bool bt_sd_begin(void);
bool bt_sd_append(const void *data, size_t count);
bool bt_sd_end(void);
FILE *bt_sd_open_pending(void);
FILE *bt_sd_open_cached(void);
bool bt_sd_commit(void);
void bt_sd_abort(void);
/* Versioned on-SD parsed metadata, validated against raw ini checksum. */
bool bt_sd_snapshot_save(const BtDefinition *d);
bool bt_sd_snapshot_load(BtDefinition *d);
/* User-selected real INI telemetry channel names, saved independently of ECU INI. */
bool bt_sd_gauges_load(char slots[2][64]);
bool bt_sd_gauges_save(const char slots[2][64]);
bool bt_sd_http_report(const char *path,const char *status_line,int code,const char *body);
bool bt_sd_signature_report(const char *ecu, const char *ini, int http_code);
bool bt_sd_preserve_unverified(void);
#endif
