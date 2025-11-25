#pragma once

#define CHARACTER_WIDTH 6
#define CHARACTER_HEIGHT 6

class IDisplay {
public:
    virtual ~IDisplay() = default;

    virtual void drawRect(float x, float y, float w, float h, unsigned char r,
                         unsigned char g, unsigned char b) = 0;
    virtual void drawCharacter(unsigned char grid[CHARACTER_HEIGHT][CHARACTER_WIDTH][3],
                             float x, float y, int hash) = 0;
    virtual void clearView(unsigned char r, unsigned char g, unsigned char b) = 0;
    virtual void clearScreen(unsigned char r, unsigned char g, unsigned char b) = 0;
    virtual void initView(int w, int h) = 0;
    virtual void pushSprite(int x, int y) = 0;
    
    virtual int getCanvasX() = 0;
    virtual int getCanvasY() = 0;
    virtual int getScreenWidth() = 0;
    virtual int getScreenHeight() = 0;
};
