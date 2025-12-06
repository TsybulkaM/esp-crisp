#include "settings_service.h"
#include "components/wifi.h"

extern "C" {

int settings_getComponentCount() {
    auto service = SettingsService::getInstance();
    return service ? service->getComponentCount() : 0;
}

const char* settings_getComponentName(int index) {
    auto service = SettingsService::getInstance();
    if (!service) return nullptr;
    
    auto component = service->getComponent(index);
    return component ? component->get_name() : nullptr;
}

void settings_toggleComponent(int index) {
    auto service = SettingsService::getInstance();
    if (!service) return;
    
    auto component = service->getComponent(index);
    if (component) {
        component->toggle_state();
    }
}

const char* settings_getComponentStateText(int index) {
    auto service = SettingsService::getInstance();
    if (!service) return "";
    
    auto component = service->getComponent(index);
    return component ? component->get_current_state_text() : "";
}

int settings_getComponentStateColor(int index) {
    auto service = SettingsService::getInstance();
    if (!service) return 0;
    
    auto component = service->getComponent(index);
    return component ? component->get_current_state_color() : 0;
}

}
