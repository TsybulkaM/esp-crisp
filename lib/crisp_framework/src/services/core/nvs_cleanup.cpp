#include "nvs_cleanup.h"
#include "../fota/fota.h"
#include "../mqtt/mqtt_service.h"
#include "core_service.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <string.h>

static const char* TAG = "nvs_cleanup";

namespace NVSCleanup {

/**
 * Clear test data from NVS and restore production defaults
 */
void clearTestData() {
    CoreService::log_info(TAG, "Clearing test data from NVS...");
    
    // Check and fix FOTA URL
    const char* fotaUrl = FotaService::getUpdateUrl();
    if (fotaUrl && (strstr(fotaUrl, "test-") || strstr(fotaUrl, ".local"))) {
        CoreService::log_warn(TAG, "Found test FOTA URL: %s, clearing...", fotaUrl);
        
        // Clear the bad URL
        nvs_handle_t handle;
        if (nvs_open("fota", NVS_READWRITE, &handle) == ESP_OK) {
            nvs_erase_key(handle, "update_url");
            nvs_commit(handle);
            nvs_close(handle);
            CoreService::log_info(TAG, "FOTA URL cleared");
        }
    }
    
    // Check and fix MQTT broker
    const char* mqttBroker = MqttService::getBrokerUri();
    if (mqttBroker && (strstr(mqttBroker, "test-") || strstr(mqttBroker, ".local"))) {
        CoreService::log_warn(TAG, "Found test MQTT broker: %s, clearing...", mqttBroker);
        
        // Clear the bad broker
        nvs_handle_t handle;
        if (nvs_open("mqtt", NVS_READWRITE, &handle) == ESP_OK) {
            nvs_erase_key(handle, "broker_uri");
            nvs_commit(handle);
            nvs_close(handle);
            CoreService::log_info(TAG, "MQTT broker cleared");
        }
    }
    
    // Check and fix FOTA version
    const char* fotaVersion = FotaService::getCurrentVersion();
    if (fotaVersion && strstr(fotaVersion, "test")) {
        CoreService::log_warn(TAG, "Found test version: %s, clearing...", fotaVersion);
        
        // Clear the test version
        nvs_handle_t handle;
        if (nvs_open("fota", NVS_READWRITE, &handle) == ESP_OK) {
            nvs_erase_key(handle, "last_version");
            nvs_commit(handle);
            nvs_close(handle);
            CoreService::log_info(TAG, "FOTA version cleared");
        }
    }
    
    CoreService::log_info(TAG, "NVS cleanup completed");
}

/**
 * Validate and fix production configuration
 */
void validateProductionConfig() {
    CoreService::log_info(TAG, "Validating production configuration...");
    
    // Ensure FOTA URL is set to production default if empty
    const char* fotaUrl = FotaService::getUpdateUrl();
    if (!fotaUrl || strlen(fotaUrl) == 0) {
        const char* defaultUrl = "http://192.168.1.179:8081/api/fota/check";
        CoreService::log_info(TAG, "Setting default FOTA URL: %s", defaultUrl);
        FotaService::saveUpdateUrl(defaultUrl);
    }
    
    CoreService::log_info(TAG, "Configuration validation completed");
}

} // namespace NVSCleanup
