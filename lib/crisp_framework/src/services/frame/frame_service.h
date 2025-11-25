#pragma once
#include "hal/display_hal.hpp"
#include "hal/controller_hal.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

class FrameService {
public:
    FrameService(IDisplay& display, IController& controller, int fps);

    static FrameService* getInstance() { return instance; }
    static void setInstance(FrameService* svc) { instance = svc; }

    void drawRect(float x, float y, float w, float h, unsigned char r,
                 unsigned char g, unsigned char b);
    void drawCharacter(unsigned char grid[CHARACTER_HEIGHT][CHARACTER_WIDTH][3],
                      float x, float y, int hash);
    void clearView(unsigned char r, unsigned char g, unsigned char b);
    void clearScreen(unsigned char r, unsigned char g, unsigned char b);
    void initView(int w, int h);
    int getScreenWidth();
    int getScreenHeight();

    void createTask();

private:
    static FrameService* instance;
    IDisplay& display;
    IController& controller;
    int fps;
    
    TaskHandle_t frameTaskHandle;
    TimerHandle_t frameTimer;

    void updateFromFrameTask();
    static void updateFrameTaskStatic(void *pvParameters);
    static void frameTimerCallbackStatic(TimerHandle_t xTimer);
};
