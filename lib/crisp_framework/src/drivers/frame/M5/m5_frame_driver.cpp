#include "m5_frame_driver.h"
#include "services/core/core_service.h"

#include <M5Unified.h>

M5FrameDriver::M5FrameDriver() 
    : canvas(&M5.Display), isCanvasCreated(false), canvasX(0), canvasY(0), characterSpritesCount(0)
{
    auto cfg = M5.config();
    cfg.external_spk = false;
    M5.begin(cfg);
    
    M5.Display.setBrightness(128);
    screenWidth = M5.Display.width();
    screenHeight = M5.Display.height();
    initCharacterSprite();
}

M5FrameDriver::~M5FrameDriver()
{
    if (isCanvasCreated)
    {
        canvas.deleteSprite();
    }
}

void M5FrameDriver::initCharacterSprite()
{
    for (int i = 0; i < MAX_CACHED_CHARACTER_PATTERN_COUNT; i++)
    {
        characterSprites[i].sprite = new LGFX_Sprite(&canvas);
    }
    characterSpritesCount = 0;
}

void M5FrameDriver::resetCharacterSprite()
{
    for (int i = 0; i < characterSpritesCount; i++)
    {
        characterSprites[i].sprite->deleteSprite();
    }
    characterSpritesCount = 0;
}

void M5FrameDriver::createCharacterImageData(
    unsigned char grid[CHARACTER_HEIGHT][CHARACTER_WIDTH][3])
{
    int cp = 0;
    for (int y = 0; y < CHARACTER_HEIGHT; y++)
    {
        for (int x = 0; x < CHARACTER_WIDTH; x++)
        {
            unsigned char r = grid[y][x][0];
            unsigned char g = grid[y][x][1];
            unsigned char b = grid[y][x][2];
            characterImageData[cp] =
                (r > 0 || g > 0 || b > 0) ? canvas.color565(r, g, b) : TRANSPARENT_COLOR;
            cp++;
        }
    }
}

void M5FrameDriver::drawRect(float x, float y, float w, float h, unsigned char r,
                unsigned char g, unsigned char b)
{
    canvas.fillRect((int)x, (int)y, (int)w, (int)h, canvas.color565(r, g, b));
}

void M5FrameDriver::drawCharacter(unsigned char grid[CHARACTER_HEIGHT][CHARACTER_WIDTH][3],
                      float x, float y, int hash)
{
    CharacterSprite *cp = NULL;
    for (int i = 0; i < characterSpritesCount; i++)
    {
        if (characterSprites[i].hash == hash)
        {
            cp = &characterSprites[i];
            break;
        }
    }
    if (cp == NULL)
    {
        cp = &characterSprites[characterSpritesCount];
        cp->hash = hash;
        createCharacterImageData(grid);
        cp->sprite->createSprite(CHARACTER_WIDTH, CHARACTER_HEIGHT);
        cp->sprite->setSwapBytes(true);
        cp->sprite->pushImage(0, 0, CHARACTER_WIDTH, CHARACTER_HEIGHT,
                              characterImageData);
        characterSpritesCount++;
    }
    cp->sprite->pushSprite((int)x, (int)y, TRANSPARENT_COLOR);
}

void M5FrameDriver::clearView(unsigned char r, unsigned char g, unsigned char b)
{
    canvas.fillScreen(canvas.color565(r, g, b));
}

void M5FrameDriver::clearScreen(unsigned char r, unsigned char g, unsigned char b)
{
    M5.Display.fillScreen(M5.Display.color565(r, g, b));
}

void M5FrameDriver::initView(int w, int h)
{
    if (isCanvasCreated)
    {
        canvas.deleteSprite();
    }
    isCanvasCreated = true;
    canvas.createSprite(w, h);
    if (w > 135)
    {
        M5.Display.setRotation(1);
    }
    else
    {
        M5.Display.setRotation(0);
    }
    canvasX = (M5.Display.width() - w) / 2;
    canvasY = (M5.Display.height() - h) / 2;
    resetCharacterSprite();
}

void M5FrameDriver::pushSprite(int x, int y)
{
    canvas.pushSprite(x, y);
}