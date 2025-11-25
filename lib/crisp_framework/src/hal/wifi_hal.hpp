#pragma once

#include <cstdint>

class IWiFi {
public:
    virtual ~IWiFi() = default;

    virtual bool startAP(const char* ssid, const char* password = nullptr) = 0;
    virtual void stopAP() = 0;
    virtual bool isAPActive() = 0;
    virtual int getAPClientCount() = 0;
    
    virtual bool startStation(const char* ssid, const char* password) = 0;
    virtual void stopStation() = 0;
    virtual bool isStationConnected() = 0;
    virtual const char* getStationIP() = 0;
};
