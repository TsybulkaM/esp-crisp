// Frame Adapter - связывает CGLP API с FrameService
#include "frame_service.h"

extern "C" {
    #include "cglp.h"
}

void md_drawRect(float x, float y, float w, float h, unsigned char r,
                 unsigned char g, unsigned char b)
{
    auto service = FrameService::getInstance();
    if (service) {
        service->drawRect(x, y, w, h, r, g, b);
    }
}

void md_drawCharacter(unsigned char grid[CHARACTER_HEIGHT][CHARACTER_WIDTH][3],
                      float x, float y, int hash)
{
    auto service = FrameService::getInstance();
    if (service) {
        service->drawCharacter(grid, x, y, hash);
    }
}

void md_clearView(unsigned char r, unsigned char g, unsigned char b)
{
    auto service = FrameService::getInstance();
    if (service) {
        service->clearView(r, g, b);
    }
}

void md_clearScreen(unsigned char r, unsigned char g, unsigned char b)
{
    auto service = FrameService::getInstance();
    if (service) {
        service->clearScreen(r, g, b);
    }
}

void md_initView(int w, int h)
{
    auto service = FrameService::getInstance();
    if (service) {
        service->initView(w, h);
    }
}

int md_getScreenWidth() {
    auto service = FrameService::getInstance();
    if (service) {
        return service->getScreenWidth();
    }
    return 0;
}

int md_getScreenHeight() {
    auto service = FrameService::getInstance();
    if (service) {
        return service->getScreenHeight();
    }
    return 0;
}