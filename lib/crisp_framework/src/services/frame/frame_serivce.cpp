#include "frame_service.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "hal/controller_hal.hpp"

extern "C" {
    #include "cglp.h"
}

FrameService* FrameService::instance = nullptr;

FrameService::FrameService(IDisplay& display, IController& controller, int fps)
    : display(display), controller(controller), fps(fps)
{
}

void FrameService::drawRect(float x, float y, float w, float h, unsigned char r,
                           unsigned char g, unsigned char b)
{
    display.drawRect(x, y, w, h, r, g, b);
}

void FrameService::drawCharacter(unsigned char grid[CHARACTER_HEIGHT][CHARACTER_WIDTH][3],
                                float x, float y, int hash)
{
    display.drawCharacter(grid, x, y, hash);
}

void FrameService::clearView(unsigned char r, unsigned char g, unsigned char b)
{
    display.clearView(r, g, b);
}

void FrameService::clearScreen(unsigned char r, unsigned char g, unsigned char b)
{
    display.clearScreen(r, g, b);
}

void FrameService::initView(int w, int h)
{
    display.initView(w, h);
}

int FrameService::getScreenWidth() {
    return display.getScreenWidth();
}

int FrameService::getScreenHeight() {
    return display.getScreenHeight();
}

void FrameService::updateFromFrameTask()
{
    controller.updateStates();
    bool ba = controller.getButtonAState();
    bool bb = controller.getButtonBState();
    setButtonState(false, false, false, false, bb, ba);
    updateFrame();
    display.pushSprite(display.getCanvasX(), display.getCanvasY());
    if (!isInMenu)
    {
        if (currentInput.b.isJustPressed && currentInput.a.isPressed)
        {
            goToMenu();
        }
    }
}

void FrameService::updateFrameTaskStatic(void *pvParameters)
{
    FrameService* service = static_cast<FrameService*>(pvParameters);
    while (1)
    {
        xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
        service->updateFromFrameTask();
    }
}

void FrameService::frameTimerCallbackStatic(TimerHandle_t xTimer)
{
    FrameService* service = static_cast<FrameService*>(pvTimerGetTimerID(xTimer));
    xTaskNotifyFromISR(service->frameTaskHandle, 0, eIncrement, NULL);
}

void FrameService::createTask() {
    xTaskCreate(updateFrameTaskStatic, "updateFrameTask", 8192, this, 1, &frameTaskHandle);

    frameTimer = xTimerCreate("frameTimer", pdMS_TO_TICKS(1000 / fps), 
                            pdTRUE, this, frameTimerCallbackStatic);

    xTimerStart(frameTimer, 0);
}