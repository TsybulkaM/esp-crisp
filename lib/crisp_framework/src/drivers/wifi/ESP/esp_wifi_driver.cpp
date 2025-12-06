#include "esp_wifi_driver.h"
#include "services/core/core_service.h"
#include <esp_netif.h>
#include <esp_mac.h>
#include <nvs_flash.h>
#include <string.h>

static const char* TAG = "ESPWiFiDriver";

ESPWiFiDriver::ESPWiFiDriver() 
    : apActive(false), stationConnected(false), initialized(false), 
      apNetif(nullptr), staNetif(nullptr) {
    stationIP[0] = '\0';
    initWiFi();
}

ESPWiFiDriver::~ESPWiFiDriver() {
    stopAP();
    stopStation();
    if (initialized) {
        esp_wifi_deinit();
    }
}

void ESPWiFiDriver::initWiFi() {
    if (initialized) {
        return;
    }
    
    CoreService::log_info(TAG, "Initializing WiFi driver...");
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    
    // Initialize TCP/IP
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    
    // Register event handler
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHandler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &eventHandler, this));
    
    initialized = true;
    CoreService::log_info(TAG, "WiFi driver initialized");
}

bool ESPWiFiDriver::startAP(const char* ssid, const char* password) {
    if (apActive) {
        CoreService::log_warn(TAG, "AP already active");
        return true;
    }
    
    CoreService::log_info(TAG, "Starting WiFi AP: %s", ssid);
    
    // Create AP netif if not exists
    if (!apNetif) {
        CoreService::log_info(TAG, "Creating default WiFi AP netif with DHCP server");
        apNetif = esp_netif_create_default_wifi_ap();
        if (!apNetif) {
            CoreService::log_error(TAG, "Failed to create AP netif!");
            return false;
        }
    } else {
        CoreService::log_info(TAG, "Reusing existing AP netif");
    }
    
    // Get and log AP IP configuration
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(apNetif, &ip_info) == ESP_OK) {
        CoreService::log_info(TAG, "AP IP: " IPSTR, IP2STR(&ip_info.ip));
        CoreService::log_info(TAG, "AP Gateway: " IPSTR, IP2STR(&ip_info.gw));
        CoreService::log_info(TAG, "AP Netmask: " IPSTR, IP2STR(&ip_info.netmask));
    } else {
        CoreService::log_error(TAG, "Failed to get AP IP info!");
    }
    
    // Ensure DHCP server is started
    esp_netif_dhcp_status_t dhcp_status;
    if (esp_netif_dhcps_get_status(apNetif, &dhcp_status) == ESP_OK) {
        if (dhcp_status != ESP_NETIF_DHCP_STARTED) {
            CoreService::log_info(TAG, "DHCP server not running, starting...");
            if (esp_netif_dhcps_start(apNetif) == ESP_OK) {
                CoreService::log_info(TAG, "DHCP server started successfully");
            } else {
                CoreService::log_error(TAG, "Failed to start DHCP server!");
            }
        } else {
            CoreService::log_info(TAG, "DHCP server already running");
        }
    } else {
        CoreService::log_warn(TAG, "Could not get DHCP status, assuming it's running");
    }
    
    // Configure AP
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = strlen(ssid);
    wifi_config.ap.channel = 1;
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    
    CoreService::log_info(TAG, "AP Config - SSID: %s, Channel: %d, Max Connections: %d, Auth: OPEN", 
        ssid, 1, 4);
    
    if (password && strlen(password) > 0) {
        strncpy((char*)wifi_config.ap.password, password, sizeof(wifi_config.ap.password) - 1);
        wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    apActive = true;
    CoreService::log_info(TAG, "WiFi AP started successfully");
    return true;
}

void ESPWiFiDriver::stopAP() {
    if (!apActive) {
        return;
    }
    
    CoreService::log_info(TAG, "Stopping WiFi AP...");
    esp_wifi_stop();
    apActive = false;
    CoreService::log_info(TAG, "WiFi AP stopped");
}

bool ESPWiFiDriver::isAPActive() {
    return apActive;
}

int ESPWiFiDriver::getAPClientCount() {
    if (!apActive) {
        return 0;
    }
    
    wifi_sta_list_t sta_list;
    esp_wifi_ap_get_sta_list(&sta_list);
    return sta_list.num;
}

bool ESPWiFiDriver::startStation(const char* ssid, const char* password) {
    if (stationConnected) {
        CoreService::log_warn(TAG, "Station already connected");
        return true;
    }
    
    CoreService::log_info(TAG, "Connecting to WiFi: %s", ssid);
    
    // Create STA netif if not exists
    if (!staNetif) {
        staNetif = esp_netif_create_default_wifi_sta();
    }
    
    // Configure STA
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    
    CoreService::log_info(TAG, "WiFi connection initiated");
    return true;
}

void ESPWiFiDriver::stopStation() {
    if (!stationConnected) {
        return;
    }
    
    CoreService::log_info(TAG, "Disconnecting from WiFi...");
    esp_wifi_disconnect();
    esp_wifi_stop();
    stationConnected = false;
    stationIP[0] = '\0';
    CoreService::log_info(TAG, "WiFi disconnected");
}

bool ESPWiFiDriver::isStationConnected() {
    return stationConnected;
}

const char* ESPWiFiDriver::getStationIP() {
    return stationIP;
}

void ESPWiFiDriver::eventHandler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data) {
    ESPWiFiDriver* driver = (ESPWiFiDriver*)arg;
    
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START:
                CoreService::log_info(TAG, "WiFi AP started");
                break;
            case WIFI_EVENT_AP_STOP:
                CoreService::log_info(TAG, "WiFi AP stopped");
                break;
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*)event_data;
                CoreService::log_info(TAG, "Client connected: " MACSTR, MAC2STR(event->mac));
                break;
            }
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*)event_data;
                CoreService::log_info(TAG, "Client disconnected: " MACSTR, MAC2STR(event->mac));
                break;
            }
            case WIFI_EVENT_STA_START:
                CoreService::log_info(TAG, "WiFi STA started");
                break;
            case WIFI_EVENT_STA_CONNECTED:
                CoreService::log_info(TAG, "WiFi connected");
                driver->stationConnected = true;
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                CoreService::log_info(TAG, "WiFi disconnected");
                driver->stationConnected = false;
                driver->stationIP[0] = '\0';
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
            snprintf(driver->stationIP, sizeof(driver->stationIP), IPSTR, IP2STR(&event->ip_info.ip));
            CoreService::log_info(TAG, "Got IP address: %s", driver->stationIP);
        }
    }
}
