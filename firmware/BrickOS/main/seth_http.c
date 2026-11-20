/*
 * BrickOS ESP-resident Seth education service.
 *
 * Shares the existing pure-C prototype, never invokes the ECU command
 * transport, never interprets unverified sensor offsets and never writes
 * physical engine/build facts to the open-access-point flash.
 *
 * Deliberately restricted to a single volatile demo session. This is NOT
 * an authenticated multi-user engine profile or an autonomous tuner.
 */
#include "seth_http.h"
#include "seth_agent.h"
#include "seth_actions.h"
#include "seth_packet_json.h"
#include "telemetry.h"
#include "ini_cache.h"
#include "vcp_tcp_gateway.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

extern void brick_ecu_signature_get(char *out, size_t cap);

#define SETH_RESPONSE_BUFFER 8192u
#define SETH_MAX_ACTION_BODY 448

static seth_agent_t s_agent;
static SemaphoreHandle_t s_agent_lock;

static esp_err_t fail(httpd_req_t *r, const char *status, const char *message) {
    httpd_resp_set_status(r, status);
    httpd_resp_set_type(r, "application/json; charset=utf-8");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    cJSON *j = cJSON_CreateObject();
    if (!j) return httpd_resp_sendstr(r, "{\"ok\":false,\"error\":\"Low memory\"}");
    cJSON_AddBoolToObject(j, "ok", false);
    cJSON_AddStringToObject(j, "error", message);
    char *body = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    esp_err_t result = httpd_resp_sendstr(r, body ? body : "{\"ok\":false}");
    free(body);
    return result;
}

static bool allowed_education_action(const char *s) {
    static const char *const allow[] = {
        "home", "full_tour", "stage", "continue", "lesson",
        "study_topic", "continue_topic", "depth", "interaction",
        "explain_alternate", "check_start", "check_answer",
        "i_understand", "report"
    };
    if (!s) return false;
    for (size_t i=0; i<sizeof(allow)/sizeof(allow[0]); ++i) {
        if (strcmp(s, allow[i]) == 0) return true;
    }
    return false;
}

void seth_http_init(void) {
    s_agent_lock = xSemaphoreCreateMutex();
    if (s_agent_lock) seth_init(&s_agent);
}

/* This metadata describes sources and freshness; it is never an engine
 * diagnosis, physical sensor validation, or a license to write calibration. */
static cJSON *make_source(bool *definition_matched) {
    char signature[160] = "";
    char ini_sha[65] = "";
    brick_ecu_signature_get(signature, sizeof(signature));
    const bool usb_ready = vcp_gateway_usb_ready();
    const bool matched = usb_ready && signature[0] &&
                         ini_cache_matches_signature(signature);
    if (definition_matched) *definition_matched = matched;
    const bool has_sha = ini_cache_sha256(ini_sha);
    brick_live_stats_t stats;
    brick_live_get_stats(&stats);
    const bool recent = usb_ready && stats.active && stats.samples_ok > 0 &&
                        stats.age_ms <= 1200u && stats.bytes_per_sample > 0;
    cJSON *source = cJSON_CreateObject();
    if (!source) return NULL;
    cJSON_AddBoolToObject(source, "ecu_connected", usb_ready && signature[0] != 0);
    cJSON_AddStringToObject(source, "ecu_signature", signature);
    cJSON_AddBoolToObject(source, "ini_cached", ini_cache_present());
    cJSON_AddBoolToObject(source, "ini_fingerprint_available", has_sha);
    cJSON_AddStringToObject(source, "ini_sha256", has_sha ? ini_sha : "");
    cJSON_AddBoolToObject(source, "ini_signature_matched", matched);
    cJSON_AddBoolToObject(source, "telemetry_fresh", recent);
    cJSON_AddNumberToObject(source, "telemetry_sequence", stats.sequence);
    cJSON_AddNumberToObject(source, "telemetry_age_ms", stats.samples_ok ? stats.age_ms : -1);
    cJSON_AddNumberToObject(source, "telemetry_actual_hz", stats.actual_millihz / 1000.0);
    cJSON_AddNumberToObject(source, "telemetry_samples_ok", stats.samples_ok);
    cJSON_AddNumberToObject(source, "telemetry_samples_failed", stats.samples_failed);
    cJSON_AddStringToObject(source, "role_binding", "unverified_no_sensor_values_decoded");
    cJSON_AddBoolToObject(source, "calibration_authority", false);
    return source;
}

static esp_err_t send_view(httpd_req_t *r) {
    if (!s_agent_lock) return fail(r, "503 Service Unavailable", "Seth agent unavailable");
    bool matched = false;
    cJSON *source = make_source(&matched);
    if (!source) return fail(r, "503 Service Unavailable", "No response memory");

    char *packet_json = malloc(SETH_RESPONSE_BUFFER);
    if (!packet_json) {
        cJSON_Delete(source);
        return fail(r, "503 Service Unavailable", "No packet memory");
    }
    size_t count = 0;
    if (xSemaphoreTake(s_agent_lock, pdMS_TO_TICKS(150)) == pdTRUE) {
        s_agent.ecu_definition_matched = matched;
        seth_packet_t p;
        seth_render(&s_agent, &p);
        count = seth_packet_json(&p, packet_json, SETH_RESPONSE_BUFFER);
        xSemaphoreGive(s_agent_lock);
    }
    if (!count) {
        free(packet_json);
        cJSON_Delete(source);
        return fail(r, "503 Service Unavailable", "Seth state unavailable or response too long");
    }
    cJSON *packet = cJSON_ParseWithLength(packet_json, count);
    free(packet_json);
    if (!packet) {
        cJSON_Delete(source);
        return fail(r, "500 Internal Server Error", "Could not encode Seth packet");
    }
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        cJSON_Delete(packet);
        cJSON_Delete(source);
        return fail(r, "503 Service Unavailable", "No response memory");
    }
    cJSON_AddStringToObject(root, "schema", "brickos.seth.esp.view.v1");
    cJSON_AddBoolToObject(root, "ok", true);
    cJSON_AddItemToObject(root, "agent", packet);
    cJSON_AddItemToObject(root, "source", source);
    cJSON_AddStringToObject(root, "session", "shared_volatile_education_only");
    cJSON_AddBoolToObject(root, "engine_archive_saved", false);
    cJSON_AddBoolToObject(root, "live_diagnosis_available", false);
    cJSON_AddBoolToObject(root, "ecu_writes_available", false);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return fail(r, "503 Service Unavailable", "No JSON memory");
    httpd_resp_set_type(r, "application/json; charset=utf-8");
    httpd_resp_set_hdr(r, "Cache-Control", "no-store");
    httpd_resp_set_hdr(r, "X-Content-Type-Options", "nosniff");
    esp_err_t result = httpd_resp_sendstr(r, json);
    free(json);
    return result;
}

esp_err_t seth_http_view(httpd_req_t *req) {
    return send_view(req);
}

esp_err_t seth_http_action(httpd_req_t *req) {
    if (!s_agent_lock) return fail(req, "503 Service Unavailable", "Seth agent unavailable");
    if (req->content_len < 2 || req->content_len > SETH_MAX_ACTION_BODY)
        return fail(req, "413 Payload Too Large", "Action body outside permitted size");
    char buf[SETH_MAX_ACTION_BODY+1];
    int total = 0;
    while (total < req->content_len) {
        int n = httpd_req_recv(req, buf+total, req->content_len-total);
        if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (n <= 0) return fail(req, "400 Bad Request", "Incomplete request");
        total += n;
    }
    buf[total] = 0;
    cJSON *body = cJSON_ParseWithLength(buf, (size_t)total);
    if (!body || !cJSON_IsObject(body)) {
        cJSON_Delete(body);
        return fail(req, "400 Bad Request", "Malformed action JSON");
    }
    const cJSON *name = cJSON_GetObjectItemCaseSensitive(body, "action");
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(body, "value");
    const cJSON *revision = cJSON_GetObjectItemCaseSensitive(body, "revision");
    const char *v = cJSON_IsString(value) ? value->valuestring : "";
    const bool valid = cJSON_IsString(name) &&
                       allowed_education_action(name->valuestring) &&
                       (value == NULL || cJSON_IsString(value)) &&
                       strlen(v) <= 159u &&
                       cJSON_IsNumber(revision) &&
                       isfinite(revision->valuedouble) &&
                       revision->valuedouble >= 0 &&
                       revision->valuedouble < (double)UINT32_MAX &&
                       revision->valuedouble == (double)(uint32_t)revision->valuedouble;
    if (!valid) {
        cJSON_Delete(body);
        return fail(req, "400 Bad Request", "Unsupported or invalid education action");
    }
    bool applied = false;
    if (xSemaphoreTake(s_agent_lock, pdMS_TO_TICKS(150)) == pdTRUE) {
        applied = seth_dispatch(&s_agent, (uint32_t)revision->valuedouble,
                                name->valuestring, v);
        xSemaphoreGive(s_agent_lock);
    }
    cJSON_Delete(body);
    if (!applied)
        return fail(req, "409 Conflict", "Action invalid or stale; reload Seth view");
    return send_view(req);
}
