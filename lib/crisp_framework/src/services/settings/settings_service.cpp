#include "settings_service.h"
#include "../mqtt/mqtt_service.h"
#include "../core/core_service.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_system.h>
#include <string.h>
#include <algorithm>

static const char* TAG = "SettingsService";
static const char* NVS_NAMESPACE = "settings";
static const char* SOUND_KEY = "sound_enabled";
static const char* WIFI_NVS_NAMESPACE = "wifi";
static const char* WIFI_SSID_KEY = "ssid";
static const char* WIFI_PASS_KEY = "password";

SettingsService* SettingsService::instance = nullptr;

static const char* HTML_PAGE = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><title>WiFi Setup</title></head>
<body style="font-family:Arial;padding:20px;max-width:400px;margin:0 auto">
<h2>M5StickC WiFi Setup</h2>
<form action="/save" method="POST">
<p><label>SSID:<br><input type="text" name="ssid" required style="width:100%;padding:8px"></label></p>
<p><label>Password:<br><input type="password" name="password" style="width:100%;padding:8px"></label></p>
<p><button type="submit" style="width:100%;padding:10px;background:#0066cc;color:white;border:none">Connect</button></p>
</form>
</body></html>
)rawliteral";

SettingsService::SettingsService(IWiFi& wifi) 
    : wifiDriver(wifi), soundEnabled(true), httpServer(nullptr), provisioningActive(false) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        uint8_t value = 1;
        nvs_get_u8(nvs_handle, SOUND_KEY, &value);
        soundEnabled = (value != 0);
        nvs_close(nvs_handle);
    }
}

SettingsService::~SettingsService() {
    stopWiFiProvisioning();
}

void SettingsService::setInstance(SettingsService* service) {
    instance = service;
}

SettingsService* SettingsService::getInstance() {
    return instance;
}

void SettingsService::setSoundEnabled(bool enabled) {
    soundEnabled = enabled;
    
    // Save to NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_u8(nvs_handle, SOUND_KEY, enabled ? 1 : 0);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
}

void SettingsService::toggleSound() {
    setSoundEnabled(!soundEnabled);
}

bool SettingsService::startWiFiProvisioning() {
    if (provisioningActive) {
        CoreService::log_warn(TAG, "Provisioning already active");
        return true;
    }
    
    CoreService::log_info(TAG, "Starting WiFi provisioning...");
    
    const char* deviceId = "M5";
    if (CoreService::getInstance()) deviceId = CoreService::getInstance()->getDeviceId();
    char apName[48];
    snprintf(apName, sizeof(apName), "M5StickC-%s", deviceId);
    if (!wifiDriver.startAP(apName, nullptr)) {
        CoreService::log_error(TAG, "Failed to start WiFi AP");
        return false;
    }
    
    if (!startHTTPServer()) {
        CoreService::log_error(TAG, "Failed to start HTTP server");
        wifiDriver.stopAP();
        return false;
    }
    
    provisioningActive = true;
    CoreService::log_info(TAG, "WiFi provisioning started successfully");
    return true;
}

void SettingsService::stopWiFiProvisioning() {
    if (!provisioningActive) {
        return;
    }
    
    CoreService::log_info(TAG, "Stopping WiFi provisioning...");
    
    stopHTTPServer();
    wifiDriver.stopAP();
    
    provisioningActive = false;
    CoreService::log_info(TAG, "WiFi provisioning stopped");
}

int SettingsService::getConnectedClients() {
    return wifiDriver.getAPClientCount();
}

// WiFi credentials management
bool SettingsService::hasWiFiCredentials() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return false;
    }
    
    size_t required_size = 0;
    err = nvs_get_str(nvs_handle, WIFI_SSID_KEY, nullptr, &required_size);
    nvs_close(nvs_handle);
    
    return (err == ESP_OK && required_size > 0);
}

bool SettingsService::saveWiFiCredentials(const char* ssid, const char* password) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "Failed to open NVS");
        return false;
    }
    
    nvs_set_str(nvs_handle, WIFI_SSID_KEY, ssid);
    nvs_set_str(nvs_handle, WIFI_PASS_KEY, password);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    
    CoreService::log_info(TAG, "WiFi credentials saved: SSID=%s", ssid);
    return true;
}

bool SettingsService::connectToWiFi() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return false;
    }
    
    char ssid[33] = {0};
    char password[65] = {0};
    size_t ssid_len = sizeof(ssid);
    size_t pass_len = sizeof(password);
    
    nvs_get_str(nvs_handle, WIFI_SSID_KEY, ssid, &ssid_len);
    nvs_get_str(nvs_handle, WIFI_PASS_KEY, password, &pass_len);
    nvs_close(nvs_handle);
    
    return wifiDriver.startStation(ssid, password);
}

bool SettingsService::isWiFiConnected() {
    return wifiDriver.isStationConnected();
}

const char* SettingsService::getWiFiIP() {
    return wifiDriver.getStationIP();
}

// HTTP Server implementation
bool SettingsService::startHTTPServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 4096;
    config.max_uri_handlers = 4;
    
    CoreService::log_info(TAG, "Starting HTTP server (free heap: %lu bytes)...", 
             (unsigned long)esp_get_free_heap_size());
    
    esp_err_t err = httpd_start(&httpServer, &config);
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return false;
    }
    
    // Register handlers
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handleRoot,
        .user_ctx = this
    };
    httpd_register_uri_handler(httpServer, &root_uri);
    
    httpd_uri_t save_uri = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = handleSave,
        .user_ctx = this
    };
    httpd_register_uri_handler(httpServer, &save_uri);
    
    CoreService::log_info(TAG, "HTTP server started on port 80");
    return true;
}

void SettingsService::stopHTTPServer() {
    if (httpServer) {
        httpd_stop(httpServer);
        httpServer = nullptr;
        CoreService::log_info(TAG, "HTTP server stopped");
    }
}

esp_err_t SettingsService::handleRoot(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_PAGE, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t SettingsService::handleSave(httpd_req_t *req) {
    SettingsService* service = (SettingsService*)req->user_ctx;
    
    char buf[256];
    int ret = httpd_req_recv(req, buf, std::min((int)req->content_len, (int)(sizeof(buf) - 1)));
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    // Parse form data
    char ssid[33] = {0};
    char password[65] = {0};
    
    char *ssid_start = strstr(buf, "ssid=");
    char *pass_start = strstr(buf, "password=");
    
    if (ssid_start) {
        ssid_start += 5;
        char *end = strchr(ssid_start, '&');
        int len = end ? (end - ssid_start) : strlen(ssid_start);
        strncpy(ssid, ssid_start, len < 32 ? len : 32);
    }
    
    if (pass_start) {
        pass_start += 9;
        strncpy(password, pass_start, 64);
    }
    
    // Save credentials
    service->saveWiFiCredentials(ssid, password);
    
    // Send success response
    const char* success = "<html><body><h1>WiFi Saved!</h1><p>Connecting to network...</p></body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, success, HTTPD_RESP_USE_STRLEN);
    
    // Stop AP and connect to WiFi
    CoreService::log_info(TAG, "Stopping provisioning and connecting to WiFi...");
    service->stopWiFiProvisioning();
    
    // Small delay to ensure HTTP response is sent
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Try to connect
    if (service->connectToWiFi()) {
        CoreService::log_info(TAG, "Successfully connected to WiFi: %s", service->getWiFiIP());
        // Start MQTT service if available and not already running
        if (CoreService::getInstance() && MqttService::getInstance() == nullptr) {
            const char* devId = CoreService::getInstance()->getDeviceId();
            auto mqtt = new MqttService(devId);
            MqttService::setInstance(mqtt);
            mqtt->start();
        }
    } else {
        CoreService::log_error(TAG, "Failed to connect to WiFi");
    }
    
    return ESP_OK;
}
