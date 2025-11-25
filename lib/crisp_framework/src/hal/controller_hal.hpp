#pragma once

class IController {
public:
    virtual ~IController() = default;

    virtual void updateStates() = 0;
    virtual bool getButtonAState() = 0;
    virtual bool getButtonBState() = 0;
};
