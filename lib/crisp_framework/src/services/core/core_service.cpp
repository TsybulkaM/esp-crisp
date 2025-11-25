#include "core_service.h"

#include <esp_system.h>
#include <esp_mac.h>
#include <esp_log.h>
#include "esp_task_wdt.h"

#include <nvs_flash.h>
#include <nvs.h>

#include <stdio.h>
#include <stdarg.h>


CoreService* CoreService::instance = nullptr;

CoreService::CoreService()
{
    // Initialize or load deviceId (persisted in NVS)
    const char* NVS_NS = "device";
    const char* NVS_KEY = "id";
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t required = sizeof(deviceId);
        if (nvs_get_str(nvs_handle, NVS_KEY, deviceId, &required) == ESP_OK) {
            // loaded
            nvs_close(nvs_handle);
        } else {
            nvs_close(nvs_handle);
            // generate and store
            uint8_t mac[6];
            esp_read_mac(mac, ESP_MAC_WIFI_STA);
            snprintf(deviceId, sizeof(deviceId), "%02X%02X%02X", mac[3], mac[4], mac[5]);
            if (nvs_open(NVS_NS, NVS_READWRITE, &nvs_handle) == ESP_OK) {
                nvs_set_str(nvs_handle, NVS_KEY, deviceId);
                nvs_commit(nvs_handle);
                nvs_close(nvs_handle);
            }
        }
    } else {
        // NVS namespace not present: generate deviceId from MAC and try to save
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(deviceId, sizeof(deviceId), "%02X%02X%02X", mac[3], mac[4], mac[5]);
        if (nvs_open(NVS_NS, NVS_READWRITE, &nvs_handle) == ESP_OK) {
            nvs_set_str(nvs_handle, NVS_KEY, deviceId);
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
        }
    }

    esp_task_wdt_deinit();
}

void CoreService::log_info(const char* tag, const char* format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    ESP_LOGI(tag, "%s", buffer);
}

void CoreService::log_debug(const char* tag, const char* format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    ESP_LOGD(tag, "%s", buffer);
}

void CoreService::log_warn(const char* tag, const char* format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    ESP_LOGW(tag, "%s", buffer);
}

void CoreService::log_error(const char* tag, const char* format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    ESP_LOGE(tag, "%s", buffer);
}
