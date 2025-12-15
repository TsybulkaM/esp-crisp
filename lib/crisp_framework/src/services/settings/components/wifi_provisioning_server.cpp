#include "wifi.h"

#include "../../core/core_service.h"       // For logging
#include "../../mqtt/mqtt_service.h"       // For MQTT broker saving
#include <esp_system.h>
#include "esp_event.h"
#include <string.h>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "WifiSettingsComponent";

static const char* HTML_PAGE = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><title>WiFi Setup</title></head>
<body style="font-family:Arial;padding:20px;max-width:400px;margin:0 auto">
<h2>M5StickC WiFi Setup</h2>
<form action="/save" method="POST">
<p><label>SSID (optional):<br><input type="text" name="ssid" style="width:100%;padding:8px"></label></p>
<p><label>Password (optional):<br><input type="password" name="password" style="width:100%;padding:8px"></label></p>
<p><label>MQTT Broker (optional):<br><input type="text" name="broker" placeholder="mqtt://192.168.1.179:1883" style="width:100%;padding:8px"></label></p>
<p><button type="submit" style="width:100%;padding:10px;background:#0066cc;color:white;border:none">Save</button></p>
</form>
</body></html>
)rawliteral";

ESP_EVENT_DEFINE_BASE(WIFI_CONFIG_EVENT);

// =============================
// ===== WiFi provisioning =====
// =============================

bool WifiSettingsComponent::startWiFiProvisioning() {
    if (provisioningActive) {
        CoreService::log_warn(TAG, "Provisioning already active");
        return true;
    }
    
    if (wifiDriver.isStationConnected()) {
        CoreService::log_info(TAG, "Disconnecting from WiFi station to free memory...");
        wifiDriver.stopStation();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    size_t free_heap = esp_get_free_heap_size();
    const size_t MIN_REQUIRED_HEAP = 6144;
    
    if (free_heap < MIN_REQUIRED_HEAP) {
        CoreService::log_error(TAG, "Insufficient memory for provisioning: %lu bytes (need %lu)", 
            (unsigned long)free_heap, (unsigned long)MIN_REQUIRED_HEAP);
    }
    
    CoreService::log_info(TAG, "Starting WiFi provisioning (free heap: %lu bytes)...", 
        (unsigned long)free_heap);
    
    const char* deviceId = "CRISPGAME";
    if (CoreService::getInstance()) deviceId = CoreService::getInstance()->getDeviceId();
    char apName[48];
    snprintf(apName, sizeof(apName), "ESP-%s", deviceId);
    
    CoreService::log_info(TAG, "Starting AP with SSID: %s", apName);
    if (!wifiDriver.startAP(apName, nullptr)) {
        CoreService::log_error(TAG, "Failed to start WiFi AP");
        return false;
    }
    CoreService::log_info(TAG, "AP started successfully");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    CoreService::log_info(TAG, "Starting HTTP server...");
    if (!startHTTPServer()) {
        CoreService::log_error(TAG, "Failed to start HTTP server");
        wifiDriver.stopAP();
        return false;
    }
    CoreService::log_info(TAG, "HTTP server started successfully");
    
    provisioningActive = true;
    CoreService::log_info(TAG, "WiFi provisioning ready - connect to '%s' and browse to 192.168.4.1", apName);
    return true;
}

void WifiSettingsComponent::stopWiFiProvisioning() {
    if (!provisioningActive) {
        return;
    }
    
    CoreService::log_info(TAG, "Stopping WiFi provisioning...");
    
    stopHTTPServer();
    wifiDriver.stopAP();
    
    provisioningActive = false;
    CoreService::log_info(TAG, "WiFi provisioning stopped");
}

int WifiSettingsComponent::getConnectedClients() {
    return wifiDriver.getAPClientCount();
}


// HTTP Server implementation
bool WifiSettingsComponent::startHTTPServer() {
    // Stop any existing server first
    if (httpServer) {
        CoreService::log_warn(TAG, "HTTP server already exists, stopping it first...");
        stopHTTPServer();
        vTaskDelay(pdMS_TO_TICKS(500));  // Wait for cleanup
    }
    
    // Free up memory before starting HTTP server
    size_t free_heap_before = esp_get_free_heap_size();
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 4096;
    config.max_uri_handlers = 12;  // We only need 2 handlers (root + save)
    config.task_priority = 5;  // Lower priority
    config.core_id = tskNO_AFFINITY;  // Let FreeRTOS choose core
    config.max_open_sockets = 3;
    config.lru_purge_enable = true;  // Enable LRU socket purging
    config.recv_wait_timeout = 5;  // Short timeout to free resources quickly
    config.send_wait_timeout = 5;
    config.backlog_conn = 1;  // Minimal backlog
    config.max_resp_headers = 8;  // Increased for browser requests
    config.uri_match_fn = httpd_uri_match_wildcard;  // Wildcard matching
    
    CoreService::log_info(TAG, "Starting HTTP server (free heap: %lu bytes)...", 
             (unsigned long)free_heap_before);
    
    // Try to trigger garbage collection
    vTaskDelay(pdMS_TO_TICKS(100));
    
    size_t free_heap_after_delay = esp_get_free_heap_size();
    if (free_heap_after_delay > free_heap_before) {
        CoreService::log_info(TAG, "Freed %lu bytes after delay", 
            (unsigned long)(free_heap_after_delay - free_heap_before));
    }
    
    // Try to start server with retry
    esp_err_t err = ESP_FAIL;
    for (int retry = 0; retry < 3; retry++) {
        err = httpd_start(&httpServer, &config);
        if (err == ESP_OK) {
            break;
        }
        
        CoreService::log_warn(TAG, "Failed to start HTTP server (attempt %d/3): %s", 
            retry + 1, esp_err_to_name(err));
        
        if (err == ESP_ERR_HTTPD_ALLOC_MEM) {
            CoreService::log_error(TAG, "Out of memory!");
            break;
        }
        
        // Wait and retry
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
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
    if (httpd_register_uri_handler(httpServer, &root_uri) != ESP_OK) {
        CoreService::log_error(TAG, "Failed to register root handler");
    }
    
    httpd_uri_t save_uri = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = handleSave,
        .user_ctx = this
    };
    if (httpd_register_uri_handler(httpServer, &save_uri) != ESP_OK) {
        CoreService::log_error(TAG, "Failed to register save handler");
    }
    
    // Wildcard handler for captive portal - catch all other requests
    httpd_uri_t wildcard_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = handleRoot,
        .user_ctx = this
    };
    if (httpd_register_uri_handler(httpServer, &wildcard_uri) != ESP_OK) {
        CoreService::log_warn(TAG, "Failed to register wildcard handler (not critical)");
    }
    
    size_t free_heap_after = esp_get_free_heap_size();
    CoreService::log_info(TAG, "HTTP server started on port 80 (heap used: %lu bytes)", 
        (unsigned long)(free_heap_after_delay - free_heap_after));
    CoreService::log_info(TAG, "Free heap now: %lu bytes", (unsigned long)free_heap_after);
    return true;
}

void WifiSettingsComponent::stopHTTPServer() {
    if (httpServer) {
        CoreService::log_info(TAG, "Stopping HTTP server...");
        esp_err_t err = httpd_stop(httpServer);
        if (err != ESP_OK) {
            CoreService::log_warn(TAG, "Failed to stop HTTP server: %s", esp_err_to_name(err));
        }
        httpServer = nullptr;
        
        // Give time for cleanup
        vTaskDelay(pdMS_TO_TICKS(200));
        CoreService::log_info(TAG, "HTTP server stopped");
    }
}

esp_err_t WifiSettingsComponent::handleRoot(httpd_req_t *req) {
    CoreService::log_info(TAG, "HTTP GET %s - serving config page", req->uri);
    
    // Set headers to prevent caching
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "close");
    
    httpd_resp_send(req, HTML_PAGE, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t WifiSettingsComponent::handleSave(httpd_req_t *req) {
    WifiSettingsComponent* service = (WifiSettingsComponent*)req->user_ctx;
    
    CoreService::log_info(TAG, "HTTP POST /save - receiving credentials");
    
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {  
        CoreService::log_error(TAG, "Failed to receive POST data: %d", ret);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    char ssid[33] = {0};
    char password[65] = {0};
    char broker[128] = {0};
    
    char *ssid_start = strstr(buf, "ssid=");
    char *pass_start = strstr(buf, "password=");
    char *broker_start = strstr(buf, "broker=");
    
    if (ssid_start) {
        ssid_start += 5;
        char *end = strchr(ssid_start, '&');
        int len = end ? (end - ssid_start) : strlen(ssid_start);
        strncpy(ssid, ssid_start, len < 32 ? len : 32);
    }
    
    if (pass_start) {
        pass_start += 9;
        char *end = strchr(pass_start, '&');
        int len = end ? (end - pass_start) : strlen(pass_start);
        strncpy(password, pass_start, len < 64 ? len : 64);
    }
    
    if (broker_start) {
        broker_start += 7;
        char *end = strchr(broker_start, '&');
        int len = end ? (end - broker_start) : strlen(broker_start);
        if (len > 0 && len < 128) {
            strncpy(broker, broker_start, len);
            // URL decode the broker URI (convert %3A to :, %2F to /, etc.)
            char decoded[128] = {0};
            size_t j = 0;
            for (size_t i = 0; broker[i] && j < 127; i++) {
                if (broker[i] == '%' && broker[i+1] && broker[i+2]) {
                    char hex[3] = {broker[i+1], broker[i+2], 0};
                    decoded[j++] = (char)strtol(hex, nullptr, 16);
                    i += 2;
                } else if (broker[i] == '+') {
                    decoded[j++] = ' ';
                } else {
                    decoded[j++] = broker[i];
                }
            }
            strncpy(broker, decoded, 127);
        }
    }
    
    // Save WiFi credentials only if SSID is provided
    if (strlen(ssid) > 0) {
        CoreService::log_info(TAG, "Saving WiFi credentials: SSID='%s'", ssid);
        service->saveWiFiCredentials(ssid, password);
    }
    
    if (strlen(broker) > 0) {
        CoreService::log_info(TAG, "Saving MQTT broker: %s", broker);
        MqttService::saveBrokerUri(broker);
    }
    
    const char* success = "<html><body><h1>Saved!</h1><p>Connecting...</p></body></html>";
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, success, HTTPD_RESP_USE_STRLEN);
    
    CoreService::log_info(TAG, "Posting WIFI_CONFIG_EVENT_UPDATED event");
    esp_event_post(WIFI_CONFIG_EVENT, WIFI_CONFIG_EVENT_UPDATED, nullptr, 0, portMAX_DELAY);
    
    return ESP_OK;
}