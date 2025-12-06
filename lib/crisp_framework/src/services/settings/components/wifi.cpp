#include "wifi.h"

#include "../../core/core_service.h"       // For logging
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <string.h>
#include <algorithm>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "WifiSettingsComponent";
static const char* WIFI_NVS_NAMESPACE = "wifi";
static const char* WIFI_SSID_KEY = "ssid";
static const char* WIFI_PASS_KEY = "password";

WifiSettingsComponent::WifiSettingsComponent(IWiFi& wifi) 
    : ISettingsComponent(2, "WiFi"),
    wifiDriver(wifi), 
    httpServer(nullptr), 
    provisioningActive(false) 
{
    esp_event_handler_instance_register(
        WIFI_CONFIG_EVENT, 
        WIFI_CONFIG_EVENT_UPDATED, 
        WifiSettingsComponent::onWifiConfigUpdated,
        this,
        nullptr
    );
}

WifiSettingsComponent::~WifiSettingsComponent() {
    stopWiFiProvisioning();
}

// ====================
// ===== WiFi FSM =====
// ====================

void WifiSettingsComponent::on_state_changed(unsigned int current_state) {
    switch (current_state) {
        case 0:
            // OFF - disconnect everything
            stopWiFiProvisioning();
            if (wifiDriver.isStationConnected()) {
                wifiDriver.stopStation();
            }
            break;
        case 1:
            // AP Mode (provisioning)
            if (wifiDriver.isStationConnected()) {
                wifiDriver.stopStation();
            }
            startWiFiProvisioning();
            break;
        case 2:
            // Connected mode
            stopWiFiProvisioning();
            connectToWiFi();
            break;
    }
}

void WifiSettingsComponent::set_default_state() {
    if (hasWiFiCredentials()) {
        set_state(2);  // Try to connect
    } else {
        set_state(0);  // OFF
    }
}

const char* WifiSettingsComponent::get_state_text(unsigned int state) const {
    switch (state) {
        case 0: return "OFF";
        case 1: return "AP";
        case 2: return isWiFiConnected() ? "ON" : "...";
        default: return "?";
    }
}

int WifiSettingsComponent::get_state_color(unsigned int state) const {
    const int RED = 1;
    const int GREEN = 2;
    const int YELLOW = 3;
    
    switch (state) {
        case 0: return RED;
        case 1: return YELLOW;
        case 2: return isWiFiConnected() ? GREEN : YELLOW;
        default: return RED;
    }
}

// ========================
// ===== WiFi control =====
// ========================

bool WifiSettingsComponent::hasWiFiCredentials() {
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

bool WifiSettingsComponent::saveWiFiCredentials(const char* ssid, const char* password) {
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

bool WifiSettingsComponent::connectToWiFi() {
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

bool WifiSettingsComponent::isWiFiConnected() const {
    return wifiDriver.isStationConnected();
}

const char* WifiSettingsComponent::getWiFiIP() const {
    return wifiDriver.getStationIP();
}

void WifiSettingsComponent::onWifiConfigUpdated(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data) {
    WifiSettingsComponent* component = (WifiSettingsComponent*)handler_arg;
    CoreService::log_info(TAG, "Event received: Config updated. Switching to connected mode...");
    
    // Switch to connected mode (state 2)
    component->set_state(2);
}