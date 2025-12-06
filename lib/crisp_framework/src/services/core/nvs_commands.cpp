#if CORE_DEBUG_LEVEL

#include "core_service.h"

#include <nvs_flash.h>
#include <nvs.h>
#include <string.h>

static const char* TAG = "NVS_CMD";


void CoreService::nvs_list_all() {
    CoreService::log_info(TAG, "=== NVS Contents ===");
    CoreService::log_info(TAG, "");
    
    // Known namespaces in the system
    const char* namespaces[] = {"device", "settings", "wifi", "mqtt"};
    const int ns_count = 4;
    
    for (int i = 0; i < ns_count; i++) {
        const char* ns = namespaces[i];
        nvs_handle_t handle;
        
        esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
        if (err == ESP_OK) {
            CoreService::log_info(TAG, "[%s] namespace:", ns);
            
            // Try to read common keys based on namespace
            if (strcmp(ns, "device") == 0) {
                char value[64];
                size_t len = sizeof(value);
                if (nvs_get_str(handle, "id", value, &len) == ESP_OK) {
                    CoreService::log_info(TAG, "  id = %s", value);
                }
            } else if (strcmp(ns, "settings") == 0) {
                uint8_t sound_enabled;
                if (nvs_get_u8(handle, "sound_enabled", &sound_enabled) == ESP_OK) {
                    CoreService::log_info(TAG, "  sound_enabled = %d", sound_enabled);
                }
            } else if (strcmp(ns, "wifi") == 0) {
                char ssid[64];
                size_t len = sizeof(ssid);
                if (nvs_get_str(handle, "ssid", ssid, &len) == ESP_OK) {
                    CoreService::log_info(TAG, "  ssid = %s", ssid);
                }
                char pass[64];
                len = sizeof(pass);
                if (nvs_get_str(handle, "password", pass, &len) == ESP_OK) {
                    CoreService::log_info(TAG, "  password = ***");
                }
            } else if (strcmp(ns, "mqtt") == 0) {
                char broker[128];
                size_t len = sizeof(broker);
                if (nvs_get_str(handle, "broker_uri", broker, &len) == ESP_OK) {
                    CoreService::log_info(TAG, "  broker_uri = %s", broker);
                }
            }
            
            nvs_close(handle);
            CoreService::log_info(TAG, "");
        }
    }
    
    CoreService::log_info(TAG, "===================");
}

void CoreService::nvs_get_string(const char* ns, const char* key) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
    
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "Failed to open namespace '%s': %s", ns, esp_err_to_name(err));
        return;
    }
    
    char value[256];
    size_t len = sizeof(value);
    err = nvs_get_str(handle, key, value, &len);
    
    if (err == ESP_OK) {
        CoreService::log_info(TAG, "[%s:%s] = '%s'", ns, key, value);
    } else {
        CoreService::log_error(TAG, "Failed to read [%s:%s]: %s", ns, key, esp_err_to_name(err));
    }
    
    nvs_close(handle);
}

void CoreService::nvs_get_int(const char* ns, const char* key) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
    
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "Failed to open namespace '%s': %s", ns, esp_err_to_name(err));
        return;
    }
    
    int32_t value;
    err = nvs_get_i32(handle, key, &value);
    
    if (err == ESP_OK) {
        CoreService::log_info(TAG, "[%s:%s] = %d", ns, key, value);
    } else {
        // Try u8
        uint8_t u8_value;
        err = nvs_get_u8(handle, key, &u8_value);
        if (err == ESP_OK) {
            CoreService::log_info(TAG, "[%s:%s] = %d", ns, key, u8_value);
        } else {
            CoreService::log_error(TAG, "Failed to read [%s:%s]: %s", ns, key, esp_err_to_name(err));
        }
    }
    
    nvs_close(handle);
}

void CoreService::nvs_set_string(const char* ns, const char* key, const char* value) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "Failed to open namespace '%s': %s", ns, esp_err_to_name(err));
        return;
    }
    
    err = nvs_set_str(handle, key, value);
    
    if (err == ESP_OK) {
        nvs_commit(handle);
        CoreService::log_info(TAG, "Set [%s:%s] = '%s'", ns, key, value);
    } else {
        CoreService::log_error(TAG, "Failed to write [%s:%s]: %s", ns, key, esp_err_to_name(err));
    }
    
    nvs_close(handle);
}

void CoreService::nvs_set_int(const char* ns, const char* key, int value) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "Failed to open namespace '%s': %s", ns, esp_err_to_name(err));
        return;
    }
    
    err = nvs_set_i32(handle, key, value);
    
    if (err == ESP_OK) {
        nvs_commit(handle);
        CoreService::log_info(TAG, "Set [%s:%s] = %d", ns, key, value);
    } else {
        CoreService::log_error(TAG, "Failed to write [%s:%s]: %s", ns, key, esp_err_to_name(err));
    }
    
    nvs_close(handle);
}

void CoreService::nvs_delete_key(const char* ns, const char* key) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "Failed to open namespace '%s': %s", ns, esp_err_to_name(err));
        return;
    }
    
    err = nvs_erase_key(handle, key);
    
    if (err == ESP_OK) {
        nvs_commit(handle);
        CoreService::log_info(TAG, "Deleted [%s:%s]", ns, key);
    } else {
        CoreService::log_error(TAG, "Failed to delete [%s:%s]: %s", ns, key, esp_err_to_name(err));
    }
    
    nvs_close(handle);
}

void CoreService::nvs_erase_namespace(const char* ns) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "Failed to open namespace '%s': %s", ns, esp_err_to_name(err));
        return;
    }
    
    err = ::nvs_erase_all(handle);
    
    if (err == ESP_OK) {
        nvs_commit(handle);
        CoreService::log_info(TAG, "Erased all keys in namespace '%s'", ns);
    } else {
        CoreService::log_error(TAG, "Failed to erase namespace '%s': %s", ns, esp_err_to_name(err));
    }
    
    nvs_close(handle);
}

void CoreService::nvs_erase_all() {
    CoreService::log_warn(TAG, "Erasing ALL NVS (factory reset)...");
    
    esp_err_t err = nvs_flash_erase();
    
    if (err == ESP_OK) {
        nvs_flash_init();
        CoreService::log_info(TAG, "NVS erased successfully. Device will restart...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    } else {
        CoreService::log_error(TAG, "Failed to erase NVS: %s", esp_err_to_name(err));
    }
}

#endif