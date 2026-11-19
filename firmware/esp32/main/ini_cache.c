#include "ini_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define INI_MAX_BYTES 1200000
#define INI_PATH "/logs/ecu.ini"
#define INI_TEMP "/logs/ecu.tmp"
static const char *TAG="brick.ini";
static SemaphoreHandle_t lock;
static bool initialized;

void ini_cache_init(void){
    lock=xSemaphoreCreateMutex();
#if CONFIG_BRICK_SD_SPI
    // SD logging has a different VFS. Mount the dedicated internal SPIFFS log partition for definitions.
    const esp_vfs_spiffs_conf_t c={.base_path="/logs",.partition_label="logs",.max_files=3,.format_if_mount_failed=false};
    esp_err_t e=esp_vfs_spiffs_register(&c);
    initialized=(e==ESP_OK);
#else
    initialized=esp_spiffs_mounted("logs");
#endif
    ESP_LOGI(TAG,"INI cache SPIFFS %s",initialized?"ready":"unavailable");
}
static bool acquire(void){return initialized && lock && xSemaphoreTake(lock,pdMS_TO_TICKS(3000))==pdTRUE;}
static bool get_size(size_t *size){struct stat st;if(stat(INI_PATH,&st)!=0 || st.st_size<=0)return false;*size=(size_t)st.st_size;return true;}
bool ini_cache_present(void){size_t s;return initialized && get_size(&s);}
size_t ini_cache_size(void){size_t s=0;if(initialized)get_size(&s);return s;}
static bool valid_definition(const char *path){
    FILE *f=fopen(path,"rb");if(!f)return false;
    char buf[4097];size_t n;bool constants=false,channels=false;
    while((n=fread(buf,1,4096,f))>0){
        buf[n]=0;
        if(strstr(buf,"[Constants]"))constants=true;
        if(strstr(buf,"[OutputChannels]"))channels=true;
        if(constants && channels)break;
        // Carry the last 32 bytes across boundaries to catch split headings.
        if(n==4096){fseek(f,-32,SEEK_CUR);}
    }
    fclose(f);
    return constants && channels;
}
static esp_err_t commit_file(void){
    if(!valid_definition(INI_TEMP))return ESP_ERR_INVALID_RESPONSE;
    // Prefer a complete old version over any incomplete new version.
    // SPIFFS rename replaces a file in the same mount on ESP-IDF's VFS.
    if(rename(INI_TEMP,INI_PATH)!=0)return ESP_FAIL;
    return ESP_OK;
}
esp_err_t ini_cache_upload(httpd_req_t *req){
    if(!acquire())return ESP_ERR_INVALID_STATE;
    if(req->content_len<100 || req->content_len>INI_MAX_BYTES){xSemaphoreGive(lock);return ESP_ERR_INVALID_SIZE;}
    FILE *f=fopen(INI_TEMP,"wb");if(!f){xSemaphoreGive(lock);return ESP_FAIL;}
    uint8_t buf[2048];int remaining=req->content_len;esp_err_t result=ESP_OK;
    while(remaining>0){
        int chunk=remaining>(int)sizeof(buf)?(int)sizeof(buf):remaining;
        int n=httpd_req_recv(req,(char*)buf,chunk);
        if(n<=0 || fwrite(buf,1,n,f)!=(size_t)n){result=ESP_FAIL;break;}
        remaining-=n;
    }
    if(fclose(f)!=0)result=ESP_FAIL;
    if(result==ESP_OK)result=commit_file();
    if(result!=ESP_OK)remove(INI_TEMP);
    xSemaphoreGive(lock);
    return result;
}
/* Network download intentionally disabled in the AP-only build.
   The ECU definition can still be loaded from SPIFFS or uploaded over the AP. */
esp_err_t ini_cache_download(const char *url, const char *ssid, const char *password,
                             char *error, size_t error_size) {
    (void)url; (void)ssid; (void)password;
    if (error && error_size) snprintf(error, error_size, "Network INI downloads are disabled (AP-only build)");
    return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t ini_cache_serve(httpd_req_t *req){
    if(!acquire())return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"INI storage unavailable");
    FILE *f=fopen(INI_PATH,"rb");
    if(!f){xSemaphoreGive(lock);return httpd_resp_send_err(req,HTTPD_404_NOT_FOUND,"No cached INI");}
    httpd_resp_set_type(req,"text/plain; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    char b[2048];size_t n;esp_err_t e=ESP_OK;
    while((n=fread(b,1,sizeof(b),f))>0){e=httpd_resp_send_chunk(req,b,n);if(e!=ESP_OK)break;}
    fclose(f);xSemaphoreGive(lock);
    if(e!=ESP_OK)return e;
    return httpd_resp_send_chunk(req,NULL,0);
}
esp_err_t ini_cache_delete(void){
    if(!acquire())return ESP_ERR_INVALID_STATE;
    esp_err_t e=(remove(INI_PATH)==0)?ESP_OK:ESP_FAIL;
    xSemaphoreGive(lock);return e;
}


/* Read-only USB SD -> SPIFFS: never replace a good cache with a wrong or
 * partially copied definition. Called only with a verified live ECU signature. */
static bool exact_signature(const char *path, const char *expected) {
    if (!expected || !expected[0] || strlen(expected) > 150) return false;
    FILE *in = fopen(path, "rb");
    if (!in) return false;
    char line[600];
    bool tuner_studio = false;
    bool megatune = false;
    bool has_ts_sig = false;
    bool ts_matches = false;
    bool mt_matches = false;
    while (fgets(line, sizeof(line), in)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p == '[') {
            tuner_studio = (strncasecmp(p, "[TunerStudio]", 13) == 0);
            megatune = (strncasecmp(p, "[MegaTune]", 10) == 0);
            continue;
        }
        if (!(tuner_studio || megatune) || strncasecmp(p, "signature", 9) != 0) continue;
        p += 9;
        while (*p == ' ' || *p == '\t') ++p;
        if (*p++ != '=') continue;
        while (*p == ' ' || *p == '\t') ++p;
        char quote = (*p == '"' || *p == '\'') ? *p++ : 0;
        char *end = p;
        while (*end && *end != '\n' && *end != '\r' && (!quote || *end != quote)) ++end;
        while (end > p && (end[-1] == ' ' || end[-1] == '\t')) --end;
        *end = 0;
        if (tuner_studio) {
            has_ts_sig = true;
            ts_matches = strcmp(p, expected) == 0;
            break;
        }
        if (megatune) mt_matches = strcmp(p, expected) == 0;
    }
    fclose(in);
    // Generated rusEFI INIs declare this in [TunerStudio]. Legacy files can
    // fall back to [MegaTune] only when no TunerStudio signature is declared.
    return has_ts_sig ? ts_matches : mt_matches;
}

esp_err_t ini_cache_import_exact_file(const char *source_path, const char *ecu_signature) {
    if (!source_path || !ecu_signature || !acquire()) return ESP_ERR_INVALID_STATE;
    esp_err_t result = ESP_FAIL;
    FILE *src = fopen(source_path, "rb");
    if (!src) goto done;
    if (fseek(src, 0, SEEK_END) != 0) goto close_src;
    long length = ftell(src);
    if (length < 100 || length > INI_MAX_BYTES || fseek(src, 0, SEEK_SET) != 0) {
        result = ESP_ERR_INVALID_SIZE; goto close_src;
    }
    if (!valid_definition(source_path) || !exact_signature(source_path, ecu_signature)) {
        result = ESP_ERR_INVALID_RESPONSE; goto close_src;
    }
    FILE *dst = fopen(INI_TEMP, "wb");
    if (!dst) goto close_src;
    char buf[1024]; size_t n; result = ESP_OK;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0) {
        if (fwrite(buf, 1, n, dst) != n) {result = ESP_FAIL; break;}
    }
    if (ferror(src)) result = ESP_FAIL;
    if (fclose(dst) != 0) result = ESP_FAIL;
    if (result == ESP_OK && !exact_signature(INI_TEMP, ecu_signature))
        result = ESP_ERR_INVALID_RESPONSE;
    if (result == ESP_OK) result = commit_file();
    if (result != ESP_OK) remove(INI_TEMP);
close_src:
    fclose(src);
done:
    xSemaphoreGive(lock);
    return result;
}

/* The two channels are resolved against the SPIFFS INI whose TunerStudio
 * signature equals the USB ECU signature. Unsupported types fail closed. */
bool ini_cache_bench_channels(const char *ecu_signature,
                              brick_bench_u16_t *rpm, brick_bench_u16_t *battery) {
    if (!ecu_signature || !rpm || !battery || !acquire()) return false;
    bool found_rpm = false, found_battery = false, outputs = false;
    if (!exact_signature(INI_PATH, ecu_signature)) goto finish;
    FILE *file = fopen(INI_PATH, "r");
    if (!file) goto finish;
    char line[512];
    while (fgets(line, sizeof(line), file)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '[') {
            outputs = strncasecmp(p, "[OutputChannels]", 16) == 0;
            continue;
        }
        if (!outputs || *p == ';' || *p == '#' || !*p) continue;
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = 0;
        char *end = eq - 1;
        while (end >= p && (*end == ' ' || *end == '\t')) *end-- = 0;
        brick_bench_u16_t *dest = NULL;
        if (!strcmp(p, "RPMValue")) dest = rpm;
        else if (!strcmp(p, "VBatt")) dest = battery;
        if (!dest) continue;
        char data_type[16]; unsigned offset = 0;
        double scale = 0.0, trans = 0.0;
        char *q = eq + 1;
        /* INI: scalar, U16, offset, "units", scale, translate */
        while (*q == ' ' || *q == '\t') q++;
        if (strncasecmp(q, "scalar", 6) != 0) continue;
        q += 6;
        if (*q++ != ',') continue;
        while (*q == ' ' || *q == '\t') q++;
        char *comma = strchr(q, ',');
        if (!comma || (size_t)(comma - q) >= sizeof(data_type)) continue;
        memcpy(data_type, q, (size_t)(comma - q));
        data_type[comma - q] = 0;
        size_t n = strlen(data_type);
        while (n && (data_type[n-1] == ' ' || data_type[n-1] == '\t')) data_type[--n] = 0;
        if (strcmp(data_type, "U16") != 0) continue;
        q = comma + 1;
        char *parsed_end;
        unsigned long off = strtoul(q, &parsed_end, 10);
        if (parsed_end == q || off > 65533 || *parsed_end != ',') continue;
        q = strchr(parsed_end + 1, ','); /* Skip quoted unit text. */
        if (!q) continue;
        scale = strtod(q + 1, &parsed_end);
        if (parsed_end == q + 1 || *parsed_end != ',') continue;
        trans = strtod(parsed_end + 1, &q);
        if (q == parsed_end + 1 || scale <= 0 || scale > 10000) continue;
        offset = (unsigned)off;
        dest->offset = offset; dest->scale = scale; dest->translate = trans;
        if (dest == rpm) found_rpm = true;
        else found_battery = true;
        if (found_rpm && found_battery) break;
    }
    fclose(file);
finish:
    xSemaphoreGive(lock);
    return found_rpm && found_battery;
}


/* The named commands in [ControllerCommands] are metadata only. In particular,
 * resolving a command name must never forward its payload to the ECU. */
bool ini_cache_command_info(const char *ecu_signature, const char *command_id,
                             brick_ini_command_info_t *out) {
    if (!command_id || !out) return false;
    size_t idlen=strlen(command_id);
    if (idlen<2 || idlen>63) return false;
    for (size_t i=0;i<idlen;i++) {
        unsigned char ch=(unsigned char)command_id[i];
        if (!(isalnum(ch) || ch=='_')) return false;
    }
    memset(out,0,sizeof(*out));
    if (!ecu_signature || !*ecu_signature || !acquire()) return false;
    if (!exact_signature(INI_PATH,ecu_signature)) {
        xSemaphoreGive(lock);
        return false;
    }
    FILE *file=fopen(INI_PATH,"r");
    if (!file) {xSemaphoreGive(lock);return false;}
    enum {SECTION_OTHER,SECTION_COMMANDS,SECTION_USER} section=SECTION_OTHER;
    char line[1024];
    while (fgets(line,sizeof(line),file)) {
        /* Skip oversized lines rather than partially parsing an INI entry. */
        if (!strchr(line,'\n') && !feof(file)) {
            int ch;
            while ((ch=fgetc(file))!=EOF && ch!='\n') {}
            continue;
        }
        char *p=line;
        while (*p==' '||*p=='\t')p++;
        if (*p=='[') {
            section=!strncasecmp(p,"[ControllerCommands]",20)?SECTION_COMMANDS:
                    !strncasecmp(p,"[UserDefined]",13)?SECTION_USER:SECTION_OTHER;
            continue;
        }
        if (*p==';'||*p=='#'||!*p||section==SECTION_OTHER) continue;
        char *equals=strchr(p,'=');
        if (!equals) continue;
        *equals=0;
        char *end=equals;
        while (end>p && (end[-1]==' '||end[-1]=='\t')) *--end=0;
        char *value=equals+1;
        while (*value==' '||*value=='\t')value++;
        if (section==SECTION_COMMANDS) {
            if (!strcmp(p,command_id) && *value=='"')out->declared=true;
        }else if(section==SECTION_USER && !strcasecmp(p,"commandButton")) {
            if (*value++!='"') continue;
            char *close=strchr(value,'"');
            if (!close)continue;
            char *ref=close+1;
            while (*ref==' '||*ref=='\t')ref++;
            if (*ref++!=',')continue;
            while (*ref==' '||*ref=='\t')ref++;
            char *stop=ref;
            while (isalnum((unsigned char)*stop)||*stop=='_')stop++;
            size_t ref_len=(size_t)(stop-ref);
            if (ref_len!=idlen||strncmp(ref,command_id,idlen))continue;
            out->shown_as_button=true;
            size_t label_len=(size_t)(close-value);
            if (label_len>=sizeof(out->label))label_len=sizeof(out->label)-1;
            memcpy(out->label,value,label_len);
            out->label[label_len]=0;
        }
    }
    bool ok=!ferror(file);
    fclose(file);
    xSemaphoreGive(lock);
    return ok;
}
