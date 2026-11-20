/* BrickOS optional ECU SD INI importer.
 * USB Mass Storage class on a separate interface of the STM32 composite device.
 * The build selects rusEFI's LUN1 (SD), NEVER its LUN0 (embedded INI disk).
 * Read-only user actions. No SD formatting, logging interruption, or write calls.
 * The existing native USB CDC host continues to own TunerStudio communication.
 */
#include "usb_sd_ini.h"
#include "ini_cache.h"
#include "vcp_tcp_gateway.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "usb/msc_host.h"
#include "usb/msc_host_vfs.h"

#define BRICK_SD_MOUNT "/ecusd"
#define BRICK_SD_RETRY_MS 15000
#define BRICK_SD_BOOT_DELAY_MS 4000
static const char *TAG = "brick.sdini";
static QueueHandle_t s_events;
static bool s_installed;
static volatile unsigned s_attempts;
static volatile unsigned s_imports;
static char s_status[128] = "USB mass-storage not started";
static volatile uint8_t s_usb_address;
static volatile bool s_device_present;
static volatile unsigned s_candidates;
static char s_last_file[128] = "";
static volatile int s_last_error;

typedef struct {bool connected; uint8_t address;} sd_event_t;
static void sd_status(const char *message) {
    snprintf(s_status, sizeof(s_status), "%s", message);
    ESP_LOGI(TAG, "%s", s_status);
}
const char *brick_sd_ini_state(void) {return s_status;}
unsigned brick_sd_ini_attempts(void) {return s_attempts;}
unsigned brick_sd_ini_imports(void) {return s_imports;}
unsigned brick_sd_ini_candidates(void) {return s_candidates;}
int brick_sd_ini_last_error(void) {return s_last_error;}
const char *brick_sd_ini_last_file(void) {return s_last_file;}

static void sd_event_cb(const msc_host_event_t *event, void *arg) {
    (void)arg;
    if (!event || !s_events) return;
    sd_event_t message = {0};
    if (event->event == MSC_DEVICE_CONNECTED) {
        message.connected = true;
        message.address = event->device.address;
    } else if (event->event == MSC_DEVICE_DISCONNECTED) {
        message.connected = false;
    } else {
        return;
    }
    (void)xQueueSend(s_events, &message, 0);
}

static bool ends_with_ini(const char *name) {
    size_t n = strlen(name);
    return n >= 4 && n <= 120 && strcasecmp(name + n - 4, ".ini") == 0;
}

/* Scan FAT root and conventional dirs; filename need only end in .ini.
 * Size cap includes firmware-generated 943,745-byte Proteus F7 definitions. */
/* Exact-match definitions win. When the ECU has another firmware/INI,
 * a lone valid INI must still be available to the read-only tuner.
 * Never silently select one of multiple mismatched candidates. */
static esp_err_t sd_copy_definition(const char *sig) {
    const char *dirs[] = {BRICK_SD_MOUNT, BRICK_SD_MOUNT "/rusEFI", BRICK_SD_MOUNT "/tunerstudio"};
    char fallback[192] = "";
    unsigned unmatched = 0;
    esp_err_t last_error = ESP_ERR_NOT_FOUND;
    for (unsigned d = 0; d < sizeof(dirs)/sizeof(dirs[0]); ++d) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (!ends_with_ini(entry->d_name)) continue;
            ++s_candidates;
            const size_t shown = strnlen(entry->d_name, sizeof(s_last_file) - 1);
            memcpy(s_last_file, entry->d_name, shown);
            s_last_file[shown] = '\0';
            char filename[192];
            int n = snprintf(filename, sizeof(filename), "%s/%s", dirs[d], entry->d_name);
            if (n <= 0 || (size_t)n >= sizeof(filename)) continue;
            struct stat st;
            if (stat(filename, &st) || !S_ISREG(st.st_mode)) {
                last_error = ESP_ERR_INVALID_RESPONSE;
                continue;
            }
            if (st.st_size < 100 || st.st_size > 1200000) {
                last_error = ESP_ERR_INVALID_SIZE;
                sd_status("SD INI rejected: file size outside 100B-1.2MB");
                continue;
            }
            if (!ini_cache_definition_file_valid(filename)) {
                last_error = ESP_ERR_INVALID_RESPONSE;
                ESP_LOGW(TAG,"SD INI missing required TunerStudio sections: %s",filename);
                continue;
            }
            if (sig && sig[0]) {
                esp_err_t exact = ini_cache_import_exact_file(filename, sig);
                if (exact == ESP_OK) {
                    closedir(dir);
                    sd_status("Matching ECU INI imported from SD");
                    return ESP_OK;
                }
                /* A true storage failure must not be papered over by trying
                 * another candidate or a relaxed signature path. */
                if (exact != ESP_ERR_INVALID_RESPONSE) {
                    last_error = exact;
                    closedir(dir);
                    return last_error;
                }
            }
            ++unmatched;
            if (unmatched == 1) snprintf(fallback, sizeof(fallback), "%s", filename);
        }
        closedir(dir);
    }
    if (unmatched == 1) {
        esp_err_t result = ini_cache_import_sd_file(fallback);
        if (result == ESP_OK) {
            snprintf(s_last_file,sizeof(s_last_file),"%s",strrchr(fallback,'/')?
                strrchr(fallback,'/')+1:fallback);
            sd_status("Valid alternate ECU INI imported (read-only until signature matches)");
        }
        return result;
    }
    if (unmatched > 1) {
        sd_status("Multiple valid INIs; no exact ECU signature match");
        return ESP_ERR_INVALID_STATE;
    }
    return last_error;
}
static esp_err_t sd_import_once(uint8_t addr, const char *signature) {
    ++s_attempts;
    s_candidates = 0;
    s_last_file[0] = 0;
    s_last_error = ESP_OK;
    sd_status("Opening ECU USB SD LUN1 (read-only)");
    msc_host_device_handle_t device = NULL;
    msc_host_vfs_handle_t mount = NULL;
    esp_err_t result = msc_host_install_device(addr, &device);
    if (result != ESP_OK) {
        s_last_error = result;
        sd_status("USB MSC device open failed; retrying");
        return result;
    }
    esp_vfs_fat_mount_config_t config = {
        .format_if_mount_failed = false, // NEVER format the ECU SD
        .max_files = 3,
        .allocation_unit_size = 4096,
    };
    result = msc_host_vfs_register(device, BRICK_SD_MOUNT, &config, &mount);
    if (result == ESP_OK) {
        sd_status("ECU SD mounted; checking INI definitions");
        result = sd_copy_definition(signature);
        s_last_error = result;
        if (result == ESP_OK) {
            ++s_imports;
            sd_status("ECU SD INI ready in ESP32 cache");
        } else {
            if (result != ESP_ERR_INVALID_STATE)
                sd_status(s_candidates ? "SD INI found but format/storage rejected" :
                          "SD mounted but no .ini files in scanned folders");
        }
    } else {
        s_last_error = result;
        sd_status("USB SD LUN1 FAT mount failed; no format attempted");
    }
    if (mount) {
        esp_err_t unmount = msc_host_vfs_unregister(mount);
        if (unmount != ESP_OK) ESP_LOGW(TAG, "SD unmount: %s", esp_err_to_name(unmount));
    }
    esp_err_t close_result = msc_host_uninstall_device(device);
    if (close_result != ESP_OK) ESP_LOGW(TAG, "MSC release: %s", esp_err_to_name(close_result));
    return result;
}

static void sd_ini_task(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(BRICK_SD_BOOT_DELAY_MS) ?: 1);
    char last_success_sig[160] = {0};
    bool imported = false;
    uint32_t next_try_tick = 0;
    sd_status("Waiting for ECU USB mass-storage interface");
    while (true) {
        sd_event_t event;
        while (xQueueReceive(s_events, &event, 0) == pdTRUE) {
            if (event.connected) {
                s_usb_address = event.address;
                s_device_present = true;
                next_try_tick = 0;
                last_success_sig[0] = 0;
                imported = false;
                sd_status("ECU USB MSC detected; scanning INI");
            } else {
                s_device_present = false;
                s_usb_address = 0;
                next_try_tick = 0;
                last_success_sig[0] = 0;
                imported = false;
                sd_status("ECU USB storage disconnected");
            }
        }
        char signature[160];
        brick_ecu_signature_get(signature, sizeof(signature));
        uint32_t tick = xTaskGetTickCount();
        bool due = next_try_tick == 0 || (int32_t)(tick - next_try_tick) >= 0;
        if (s_device_present && due &&
                (!imported || (signature[0] && strcmp(signature, last_success_sig) != 0))) {
            // Do not continuously re-probe an SD card in active logging use.
            next_try_tick = tick + pdMS_TO_TICKS(BRICK_SD_RETRY_MS);
            esp_err_t e = sd_import_once(s_usb_address, signature);
            if (e == ESP_OK) {
                imported = true;
                snprintf(last_success_sig, sizeof(last_success_sig), "%s", signature);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200) ?: 1);
    }
}

esp_err_t brick_sd_ini_host_install(void) {
    if (s_installed) return ESP_OK;
    sd_status("Installing USB MSC LUN1 reader");
    s_events = xQueueCreate(8, sizeof(sd_event_t));
    if (!s_events) return ESP_ERR_NO_MEM;
    msc_host_driver_config_t config = {
        .create_backround_task = true,
        .task_priority = 3,
        .stack_size = 8192,
        .core_id = 1,
        .callback = sd_event_cb,
        .callback_arg = NULL,
    };
    esp_err_t e = msc_host_install(&config);
    if (e != ESP_OK) {
        s_last_error = e;
        sd_status("USB MSC driver installation failed");
        vQueueDelete(s_events);
        s_events = NULL;
        return e;
    }
    if (xTaskCreatePinnedToCore(sd_ini_task, "brick-sd-ini", 8192, NULL, 2, NULL, 1) != pdPASS) {
        sd_status("SD INI task creation failed");
        return ESP_ERR_NO_MEM;
    }
    s_installed = true;
    sd_status("USB MSC active; waiting for ECU SD interface");
    return ESP_OK;
}
