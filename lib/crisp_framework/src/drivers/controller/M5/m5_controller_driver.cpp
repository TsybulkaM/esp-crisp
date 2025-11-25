#include "m5_controller_driver.h"

#include <M5Unified.h>

void M5ControllerDriver::updateStates() {
    M5.update();
}

bool M5ControllerDriver::getButtonAState() {
    return M5.BtnA.isPressed();
}

bool M5ControllerDriver::getButtonBState() {
    return M5.BtnB.isPressed();
}