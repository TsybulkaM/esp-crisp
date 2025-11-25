#include "settings_service.h"

extern "C" {

void settings_toggleSound() {
    auto service = SettingsService::getInstance();
    if (service) {
        service->toggleSound();
    }
}

bool settings_isSoundEnabled() {
    auto service = SettingsService::getInstance();
    return service ? service->isSoundEnabled() : true;
}

void settings_startWiFiProvisioning() {
    auto service = SettingsService::getInstance();
    if (service) {
        service->startWiFiProvisioning();
    }
}

void settings_stopWiFiProvisioning() {
    auto service = SettingsService::getInstance();
    if (service) {
        service->stopWiFiProvisioning();
    }
}

bool settings_isWiFiProvisioningActive() {
    auto service = SettingsService::getInstance();
    return service ? service->isWiFiProvisioningActive() : false;
}

bool settings_isWiFiConnected() {
    auto service = SettingsService::getInstance();
    return service ? service->isWiFiConnected() : false;
}

const char* settings_getWiFiIP() {
    auto service = SettingsService::getInstance();
    return service ? service->getWiFiIP() : "0.0.0.0";
}

}
