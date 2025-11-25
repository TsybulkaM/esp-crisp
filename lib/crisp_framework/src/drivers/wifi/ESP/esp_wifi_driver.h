#pragma once

#include "hal/wifi_hal.hpp"
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>

class ESPWiFiDriver : public IWiFi {
private:
    bool apActive;
    bool stationConnected;
    bool initialized;
    char stationIP[16];
    
    esp_netif_t* apNetif;
    esp_netif_t* staNetif;
    
    void initWiFi();
    static void eventHandler(void* arg, esp_event_base_t event_base, 
                            int32_t event_id, void* event_data);

public:
    ESPWiFiDriver();
    ~ESPWiFiDriver() override;

    bool startAP(const char* ssid, const char* password = nullptr) override;
    void stopAP() override;
    bool isAPActive() override;
    int getAPClientCount() override;
    
    bool startStation(const char* ssid, const char* password) override;
    void stopStation() override;
    bool isStationConnected() override;
    const char* getStationIP() override;
};
