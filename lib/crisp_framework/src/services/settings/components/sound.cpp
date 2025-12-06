#include "sound.h"

extern "C" {
    #include "cglp.h"
}
#include <nvs.h>
#include <esp_system.h>

static const char* SOUND_KEY = "sound_enabled";

void SoundSettingsComponent::set_default_state() {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        uint8_t value = 1;  // Default enabled
        nvs_get_u8(nvs_handle, SOUND_KEY, &value);
        set_state(value != 0 ? 1 : 0);
        nvs_close(nvs_handle);
    } else {
        set_state(1);  // Default enabled
    }
}

void SoundSettingsComponent::on_state_changed(unsigned int current_state) {
    bool enabled = (current_state == 1);
    
    if (enabled) {
        enableSound();      // cglp function
    } else {
        disableSound();     // cglp function
    }
    
    // Save to NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_u8(nvs_handle, SOUND_KEY, enabled ? 1 : 0);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }
}

const char* SoundSettingsComponent::get_state_text(unsigned int state) const {
    return (state == 1) ? "ON" : "OFF";
}

int SoundSettingsComponent::get_state_color(unsigned int state) const {
    return (state == 1) ? GREEN : RED;
}