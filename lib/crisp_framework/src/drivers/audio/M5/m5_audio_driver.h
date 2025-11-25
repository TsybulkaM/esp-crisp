#pragma once
#include "hal/audio_hal.hpp"
#include <cinttypes>

class M5BuzzerDriver : public IBuzzer {
public:
    M5BuzzerDriver();
    void tone(uint16_t freq, uint32_t duration_ms) override;
    void stop() override;
};
