#include "settings_service.h"
#include "components/sound.h"
#include "components/wifi.h"
#include "../core/core_service.h"
#include <string.h>

SettingsService* SettingsService::instance = nullptr;

SettingsService::SettingsService(IWiFi& wifi) {
    // Create and add components
    auto soundComponent = new SoundSettingsComponent();
    auto wifiComponent = new WifiSettingsComponent(wifi);
    
    addComponent(soundComponent);
    addComponent(wifiComponent);
    
    // Initialize all components to their default state
    for (auto component : components) {
        component->set_default_state();
    }
}

SettingsService::~SettingsService() {
    for (auto component : components) {
        delete component;
    }
    components.clear();
}

void SettingsService::setInstance(SettingsService* service) {
    instance = service;
}

SettingsService* SettingsService::getInstance() {
    return instance;
}

void SettingsService::addComponent(ISettingsComponent* component) {
    if (component) {
        components.push_back(component);
    }
}

ISettingsComponent* SettingsService::getComponent(int index) {
    if (index >= 0 && index < components.size()) {
        return components[index];
    }
    return nullptr;
}

ISettingsComponent* SettingsService::getComponentByName(const char* name) {
    for (auto component : components) {
        if (strcmp(component->get_name(), name) == 0) {
            return component;
        }
    }
    return nullptr;
}