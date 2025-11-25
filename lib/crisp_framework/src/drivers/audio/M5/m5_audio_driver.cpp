#include "m5_audio_driver.h"

#include <M5Unified.h>
#include <cinttypes>

M5BuzzerDriver::M5BuzzerDriver() {
    M5.Speaker.begin();
    M5.Speaker.setVolume(128);
}

void M5BuzzerDriver::tone(uint16_t freq, uint32_t duration_ms) {
    if (M5.Speaker.getVolume() == 0) {
        M5.Speaker.setVolume(128);
    }
    
    M5.Speaker.tone(freq, duration_ms);
}

void M5BuzzerDriver::stop() {
    M5.Speaker.stop();
}
