#include "fota.h"
#include "../core/core_service.h"
#include "../settings/settings_service.h"
#include "../interfaces/service_manager.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_ota_ops.h>
#include <esp_app_format.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_crt_bundle.h>
#include <cJSON.h>

static const char* TAG = "fota";
static const char* FOTA_NVS_NAMESPACE = "fota";
static const char* FOTA_URL_KEY = "update_url";
static const char* FOTA_VERSION_KEY = "last_version";

FotaService* FotaService::instance = nullptr;

FotaService::FotaService(const char* url) 
    : currentState(STATE_IDLE), 
      checkTask(nullptr), 
      lastCheckTime(0),
      checkIntervalMs(60000),  // 1 minute
      isRunning_(false) 
{
    strncpy(updateUrl, url, sizeof(updateUrl) - 1);
}

FotaService::~FotaService() {
    stop();
}

bool FotaService::start() {
    if (isRunning_) {
        CoreService::log_warn(TAG, "FOTA service already running");
        return true;
    }
    
    CoreService::log_info(TAG, "Starting FOTA service (version: %s, check interval: %ds)", 
        getCurrentVersion(), checkIntervalMs / 1000);
    
    isRunning_ = true;
    changeState(STATE_IDLE);
    
    // Create check task with larger stack for JSON parsing
    BaseType_t result = xTaskCreate(
        checkTaskFunc,
        "fota_check",
        8192,  // Increased from 4096 for JSON parsing and HTTP operations
        this,
        10,  // Higher priority to ensure OTA completes
        &checkTask
    );
    
    if (result != pdPASS) {
        CoreService::log_error(TAG, "Failed to create check task");
        isRunning_ = false;
        return false;
    }
    
    CoreService::log_info(TAG, "FOTA service started");
    return true;
}

void FotaService::stop() {
    if (!isRunning_) {
        return;
    }
    
    CoreService::log_info(TAG, "Stopping FOTA service...");
    isRunning_ = false;
    
    if (checkTask) {
        vTaskDelete(checkTask);
        checkTask = nullptr;
    }
    
    CoreService::log_info(TAG, "FOTA service stopped");
}

void FotaService::checkNow() {
    if (currentState == STATE_IDLE) {
        CoreService::log_info(TAG, "Manual update check triggered");
        changeState(STATE_CHECK_WIFI);
    }
}

void FotaService::changeState(State newState) {
    if (currentState != newState) {
        CoreService::log_info(TAG, "FSM: %d -> %d", currentState, newState);
        currentState = newState;
    }
}

bool FotaService::isWiFiConnected() {
    // Try to get WiFi component from settings service
    auto settingsService = SettingsService::getInstance();
    if (!settingsService) {
        return false;
    }
    
    // Check WiFi connection status
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        return false;
    }
    
    if (mode != WIFI_MODE_STA && mode != WIFI_MODE_APSTA) {
        return false;
    }
    
    wifi_ap_record_t ap_info;
    return esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
}

void FotaService::checkTaskFunc(void* param) {
    FotaService* service = (FotaService*)param;
    
    while (service->isRunning_) {
        service->processFSM();
        vTaskDelay(pdMS_TO_TICKS(1000));  // Check FSM every second
    }
    
    vTaskDelete(nullptr);
}

void FotaService::processFSM() {
    int64_t now = esp_timer_get_time() / 1000;  // Convert to milliseconds
    
    switch (currentState) {
        case STATE_IDLE: {
            // Check if it's time for update check
            if (now - lastCheckTime >= checkIntervalMs) {
                changeState(STATE_CHECK_WIFI);
            }
            break;
        }
        
        case STATE_CHECK_WIFI: {
            if (!isWiFiConnected()) {
                CoreService::log_warn(TAG, "WiFi not connected, waiting...");
                lastCheckTime = now;
                changeState(STATE_IDLE);
            } else {
                CoreService::log_info(TAG, "Checking for updates...");
                
                // Validate update URL before attempting check
                if (strlen(updateUrl) == 0 || 
                    strstr(updateUrl, "test-server") != nullptr ||
                    strstr(updateUrl, ".local") != nullptr) {
                    CoreService::log_warn(TAG, "Invalid or test update URL, skipping check");
                    lastCheckTime = now;
                    changeState(STATE_IDLE);
                    break;
                }
                
                // Stop MQTT before version check to free network resources
                auto serviceManager = ServiceManager::getInstance();
                if (serviceManager) {
                    CoreService::log_info(TAG, "Temporarily stopping MQTT for version check...");
                    serviceManager->requestServiceStop("mqtt", nullptr);
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
                
                changeState(STATE_CHECKING_UPDATE);
            }
            break;
        }
        
        case STATE_CHECKING_UPDATE: {
            char downloadUrl[256] = {0};
            
            if (checkForUpdate(downloadUrl, sizeof(downloadUrl))) {
                // New version available
                CoreService::log_info(TAG, "New version available, starting download...");
                changeState(STATE_DOWNLOADING);
                
                // MQTT already stopped before version check
                vTaskDelay(pdMS_TO_TICKS(200));
                
                char newVersion[32] = {0};
                if (downloadAndInstall(downloadUrl, newVersion, sizeof(newVersion))) {
                    CoreService::log_info(TAG, "OTA update successful! Rebooting...");
                    
                    // Save new version to NVS before reboot
                    saveCurrentVersion(newVersion);
                    
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                } else {
                    CoreService::log_error(TAG, "OTA update failed");
                    
                    // Restart MQTT if OTA failed
                    auto serviceManager = ServiceManager::getInstance();
                    if (serviceManager) {
                        CoreService::log_info(TAG, "Restarting MQTT service...");
                        serviceManager->requestServiceStart("mqtt", nullptr);
                    }
                    
                    changeState(STATE_ERROR);
                }
            } else {
                // No update available or error
                CoreService::log_info(TAG, "No update available or check failed");
                
                // Restart MQTT if it was stopped
                auto serviceManager = ServiceManager::getInstance();
                if (serviceManager) {
                    CoreService::log_info(TAG, "Restarting MQTT service...");
                    serviceManager->requestServiceStart("mqtt", nullptr);
                }
                
                lastCheckTime = now;
                changeState(STATE_IDLE);
            }
            break;
        }
        
        case STATE_DOWNLOADING:
            // Handled in STATE_CHECKING_UPDATE
            break;
            
        case STATE_ERROR: {
            // Wait a bit before retrying
            lastCheckTime = now;
            changeState(STATE_IDLE);
            break;
        }
    }
}

bool FotaService::checkForUpdate(char* outDownloadUrl, size_t urlSize) {
    if (strlen(updateUrl) == 0) {
        CoreService::log_warn(TAG, "Update URL not configured");
        return false;
    }
    
    // Get device ID
    auto coreService = CoreService::getInstance();
    const char* deviceId = coreService ? coreService->getDeviceId() : "unknown";
    
    // Build URL with query parameters
    char fullUrl[384];
    snprintf(fullUrl, sizeof(fullUrl), "%s?current_version=%s&device_id=%s", 
             updateUrl, getCurrentVersion(), deviceId);
    
    CoreService::log_info(TAG, "Checking update at: %s", fullUrl);
    
    esp_http_client_config_t config = {};
    config.url = fullUrl;
    config.timeout_ms = 30000;  // 30 seconds for version check
    config.buffer_size = 2048;
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        CoreService::log_error(TAG, "Failed to initialize HTTP client");
        return false;
    }
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "Failed to open connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return false;
    }
    
    int content_length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    
    CoreService::log_info(TAG, "HTTP Status: %d, Content-Length: %d", status, content_length);
    
    bool updateAvailable = false;
    
    if (status == 200) {
        if (content_length > 0 && content_length < 4096) {
            char* buffer = (char*)malloc(content_length + 1);
            if (buffer) {
                int read_len = esp_http_client_read(client, buffer, content_length);
                if (read_len > 0) {
                    buffer[read_len] = '\0';
                    CoreService::log_info(TAG, "Response: %s", buffer);
                    
                    // Parse JSON response
                    cJSON* json = cJSON_Parse(buffer);
                    if (json) {
                        cJSON* status_obj = cJSON_GetObjectItem(json, "status");
                        
                        if (status_obj && cJSON_IsString(status_obj) && 
                            strcmp(status_obj->valuestring, "update_available") == 0) {
                            
                            cJSON* version = cJSON_GetObjectItem(json, "version");
                            cJSON* download_url = cJSON_GetObjectItem(json, "download_url");
                            cJSON* file_size = cJSON_GetObjectItem(json, "file_size");
                            cJSON* checksum = cJSON_GetObjectItem(json, "checksum");
                            
                            if (version && cJSON_IsString(version) && 
                                download_url && cJSON_IsString(download_url)) {
                                
                                const char* current = getCurrentVersion();
                                CoreService::log_info(TAG, "Server version: %s, Current version: %s", 
                                    version->valuestring, current);
                                
                                // Compare versions - only proceed if different
                                if (strcmp(version->valuestring, current) == 0) {
                                    CoreService::log_info(TAG, "Versions are identical, skipping update");
                                    cJSON_Delete(json);
                                    free(buffer);
                                    esp_http_client_close(client);
                                    esp_http_client_cleanup(client);
                                    return false;
                                }
                                
                                if (file_size && cJSON_IsNumber(file_size)) {
                                    CoreService::log_info(TAG, "File size: %d bytes", file_size->valueint);
                                }
                                
                                if (checksum && cJSON_IsString(checksum)) {
                                    CoreService::log_info(TAG, "Checksum: %s", checksum->valuestring);
                                }
                                
                                // Build full download URL
                                if (download_url->valuestring[0] == '/') {
                                    // Relative URL, need to build full URL
                                    char* scheme_end = strstr(updateUrl, "://");
                                    if (scheme_end) {
                                        scheme_end += 3;
                                        char* path_start = strchr(scheme_end, '/');
                                        size_t base_len = path_start ? (path_start - updateUrl) : strlen(updateUrl);
                                        
                                        snprintf(outDownloadUrl, urlSize, "%.*s%s&device_id=%s", 
                                                (int)base_len, updateUrl, download_url->valuestring, deviceId);
                                    } else {
                                        strncpy(outDownloadUrl, download_url->valuestring, urlSize - 1);
                                    }
                                } else {
                                    strncpy(outDownloadUrl, download_url->valuestring, urlSize - 1);
                                }
                                
                                updateAvailable = true;
                            }
                        } else if (status_obj && cJSON_IsString(status_obj) && 
                                   strcmp(status_obj->valuestring, "no_update") == 0) {
                            CoreService::log_info(TAG, "Device is up to date: %s", getCurrentVersion());
                        } else {
                            CoreService::log_warn(TAG, "Unknown status in response");
                        }
                        
                        cJSON_Delete(json);
                    } else {
                        CoreService::log_error(TAG, "Failed to parse JSON response: %s", buffer);
                    }
                } else {
                    CoreService::log_error(TAG, "Failed to read response, read_len: %d", read_len);
                }
                
                free(buffer);
            } else {
                CoreService::log_error(TAG, "Failed to allocate buffer for response");
            }
        } else {
            CoreService::log_warn(TAG, "Invalid content length: %d", content_length);
        }
    } else if (status == 204) {
        CoreService::log_info(TAG, "Server reports no update available (204)");
    } else {
        CoreService::log_warn(TAG, "Unexpected HTTP status: %d", status);
    }
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return updateAvailable;
}

bool FotaService::downloadAndInstall(const char* downloadUrl, char* outVersion, size_t versionSize) {
    CoreService::log_info(TAG, "Starting OTA download from: %s", downloadUrl);
    
    // Check if URL is HTTP or HTTPS
    bool isHttps = strncmp(downloadUrl, "https://", 8) == 0;
    
    esp_http_client_config_t ota_config = {};
    ota_config.url = downloadUrl;
    ota_config.timeout_ms = 60000;  // 60 seconds timeout
    ota_config.buffer_size = 4096;  // Larger buffer for faster download
    ota_config.buffer_size_tx = 2048;
    ota_config.keep_alive_enable = true;
    ota_config.disable_auto_redirect = false;
    
    if (isHttps) {
        // For HTTPS, use system CA bundle
        ota_config.crt_bundle_attach = esp_crt_bundle_attach;
    } else {
        // For HTTP, explicitly set cert_pem to empty string to bypass verification
        static const char empty_cert[] = "";
        ota_config.cert_pem = empty_cert;
        ota_config.skip_cert_common_name_check = true;
    }
    
    esp_https_ota_config_t https_ota_config = {};
    https_ota_config.http_config = &ota_config;
    https_ota_config.bulk_flash_erase = true;  // Erase flash in bulk for speed
    https_ota_config.partial_http_download = false;
    
    CoreService::log_info(TAG, "Connecting to server...");
    
    esp_https_ota_handle_t https_ota_handle = nullptr;
    esp_err_t err = esp_https_ota_begin(&https_ota_config, &https_ota_handle);
    
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "OTA begin failed: %s", esp_err_to_name(err));
        return false;
    }
    
    CoreService::log_info(TAG, "Connected, validating firmware...");
    
    esp_app_desc_t new_app_info;
    err = esp_https_ota_get_img_desc(https_ota_handle, &new_app_info);
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "Failed to get new app info: %s", esp_err_to_name(err));
        esp_https_ota_abort(https_ota_handle);
        return false;
    }
    
    CoreService::log_info(TAG, "New firmware version: %s", new_app_info.version);
    CoreService::log_info(TAG, "Project name: %s", new_app_info.project_name);
    
    // Save new version for later
    strncpy(outVersion, new_app_info.version, versionSize - 1);
    outVersion[versionSize - 1] = '\0';
    
    // Get total image size for progress tracking
    int total_size = esp_https_ota_get_image_size(https_ota_handle);
    CoreService::log_info(TAG, "Total firmware size: %d bytes", total_size);
    
    // Download firmware
    int lastProgress = 0;
    int progressPercent = 0;
    
    while (true) {
        err = esp_https_ota_perform(https_ota_handle);
        
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        
        // Log progress
        int progress = esp_https_ota_get_image_len_read(https_ota_handle);
        
        // Log every 100KB or 10% progress
        if (progress - lastProgress > 102400 || 
            (total_size > 0 && (progress * 100 / total_size) > progressPercent + 10)) {
            
            if (total_size > 0) {
                progressPercent = progress * 100 / total_size;
                CoreService::log_info(TAG, "Downloaded: %d/%d bytes (%d%%)", 
                    progress, total_size, progressPercent);
            } else {
                CoreService::log_info(TAG, "Downloaded: %d bytes", progress);
            }
            lastProgress = progress;
        }
        
        // Yield to other tasks
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "OTA download failed: %s", esp_err_to_name(err));
        esp_https_ota_abort(https_ota_handle);
        return false;
    }
    
    // Finalize OTA
    err = esp_https_ota_finish(https_ota_handle);
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "OTA finish failed: %s", esp_err_to_name(err));
        return false;
    }
    
    CoreService::log_info(TAG, "OTA update completed successfully");
    return true;
}

// Config helpers
const char* FotaService::getUpdateUrl() {
    static char url[256] = {0};
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(FOTA_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return "";
    }
    
    size_t required_size = sizeof(url);
    nvs_get_str(nvs_handle, FOTA_URL_KEY, url, &required_size);
    nvs_close(nvs_handle);
    
    return url;
}

bool FotaService::saveUpdateUrl(const char* url) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(FOTA_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "Failed to open NVS for URL: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_str(nvs_handle, FOTA_URL_KEY, url);
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "Failed to set URL in NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "Failed to commit NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    nvs_close(nvs_handle);
    CoreService::log_info(TAG, "Update URL saved: %s", url);
    return true;
}

const char* FotaService::getCurrentVersion() {
    static char version[32] = {0};
    
    // First try to read from NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(FOTA_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t required_size = sizeof(version);
        err = nvs_get_str(nvs_handle, FOTA_VERSION_KEY, version, &required_size);
        nvs_close(nvs_handle);
        
        if (err == ESP_OK && strlen(version) > 0) {
            return version;
        }
    }
    
    // Fallback to app descriptor
    const esp_app_desc_t* app_desc = esp_app_get_description();
    strncpy(version, app_desc->version, sizeof(version) - 1);
    return version;
}

bool FotaService::saveCurrentVersion(const char* version) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(FOTA_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "Failed to open NVS for version save: %s", esp_err_to_name(err));
        return false;
    }
    
    err = nvs_set_str(nvs_handle, FOTA_VERSION_KEY, version);
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "Failed to save version to NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "Failed to commit version to NVS: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return false;
    }
    
    nvs_close(nvs_handle);
    CoreService::log_info(TAG, "Version saved to NVS: %s", version);
    return true;
}
