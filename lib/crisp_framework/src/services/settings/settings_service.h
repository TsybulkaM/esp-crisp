#pragma once

#include "drivers/wifi/ESP/esp_wifi_driver.h"
#include "components/settings_component.h"
#include <vector>

class SettingsService {
private:
    static SettingsService* instance;
    std::vector<ISettingsComponent*> components;

public:
    SettingsService(IWiFi& wifi);
    ~SettingsService();

    static void setInstance(SettingsService* service);
    static SettingsService* getInstance();

    // Component management
    void addComponent(ISettingsComponent* component);
    ISettingsComponent* getComponent(int index);
    ISettingsComponent* getComponentByName(const char* name);
    int getComponentCount() const { return components.size(); }
};
