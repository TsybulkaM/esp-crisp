#pragma once
#include "hal/controller_hal.hpp"

class M5ControllerDriver : public IController {
public:
    void updateStates() override;
    bool getButtonAState() override;
    bool getButtonBState() override;
};