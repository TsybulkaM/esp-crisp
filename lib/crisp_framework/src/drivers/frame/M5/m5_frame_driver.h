#pragma once
#include "hal/display_hal.hpp"

#include <M5Unified.h>

#define TRANSPARENT_COLOR 0
#define MAX_CACHED_CHARACTER_PATTERN_COUNT 128

typedef struct
{
    LGFX_Sprite *sprite;
    int hash;
} CharacterSprite;

class M5FrameDriver : public IDisplay {
public:
    M5FrameDriver();
    ~M5FrameDriver();

    void drawRect(float x, float y, float w, float h, unsigned char r,
                 unsigned char g, unsigned char b) override;
    void drawCharacter(unsigned char grid[CHARACTER_HEIGHT][CHARACTER_WIDTH][3],
                      float x, float y, int hash) override;
    void clearView(unsigned char r, unsigned char g, unsigned char b) override;
    void clearScreen(unsigned char r, unsigned char g, unsigned char b) override;
    void initView(int w, int h) override;
    void pushSprite(int x, int y) override;
    
    int getCanvasX() override { return canvasX; }
    int getCanvasY() override { return canvasY; }
    int getScreenWidth() override { return screenWidth; }
    int getScreenHeight() override { return screenHeight; }

private:
    M5Canvas canvas;
    bool isCanvasCreated;
    int canvasX;
    int canvasY;
    int screenWidth;
    int screenHeight;
    
    CharacterSprite characterSprites[MAX_CACHED_CHARACTER_PATTERN_COUNT];
    int characterSpritesCount;
    uint16_t characterImageData[CHARACTER_WIDTH * CHARACTER_HEIGHT];
    
    void initCharacterSprite();
    void resetCharacterSprite();
    void createCharacterImageData(unsigned char grid[CHARACTER_HEIGHT][CHARACTER_WIDTH][3]);
};