#pragma once

#include <cstdint>

class IBuzzer {
public:
    virtual ~IBuzzer() = default;

    virtual void tone(uint16_t freq, uint32_t duration_ms) = 0;
    virtual void stop() = 0;
};