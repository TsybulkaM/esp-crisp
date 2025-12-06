#pragma once

#include "hal/wifi_hal.hpp"
#include <esp_http_server.h>

class SettingsService {
private:
    static SettingsService* instance;
    IWiFi& wifiDriver;
    bool soundEnabled;
    
    // HTTP server for provisioning
    httpd_handle_t httpServer;
    bool provisioningActive;
    
    // HTTP handlers
    static esp_err_t handleRoot(httpd_req_t *req);
    static esp_err_t handleSave(httpd_req_t *req);
    
    bool startHTTPServer();
    void stopHTTPServer();

public:
    SettingsService(IWiFi& wifi);
    ~SettingsService();

    static void setInstance(SettingsService* service);
    static SettingsService* getInstance();

    // Sound settings
    bool isSoundEnabled() const { return soundEnabled; }
    void setSoundEnabled(bool enabled);
    void toggleSound();

    // WiFi provisioning
    bool startWiFiProvisioning();
    void stopWiFiProvisioning();
    bool isWiFiProvisioningActive() const { return provisioningActive; }
    int getConnectedClients();
    
    // WiFi credentials management
    bool hasWiFiCredentials();
    bool saveWiFiCredentials(const char* ssid, const char* password);
    bool connectToWiFi();
    bool isWiFiConnected();
    const char* getWiFiIP();
};
