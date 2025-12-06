#include "menu.h"

#include "cglp.h"

#define MAX_GAME_COUNT 16
#define KEY_REPEAT_DURATION 30

// External settings functions - Generic API
extern int settings_getComponentCount();
extern const char* settings_getComponentName(int index);
extern void settings_toggleComponent(int index);
extern const char* settings_getComponentStateText(int index);
extern int settings_getComponentStateColor(int index);

// Game menu
int gameCount = 0;
static Game games[MAX_GAME_COUNT];
static int gameIndex = 1;
static int gameKeyRepeatTicks = 0;

// Settings menu
static int settingsIndex = 0;
static int settingsKeyRepeatTicks = 0;

// Settings mode update function
static void updateSettingsMode() {
  // Double tap B to go back to main menu
  if (checkDoubleClick(&input.b)) {
    setMode(0); // Back to main menu
    settingsIndex = 0;
    return;
  }
  
  color = BLUE;
  text("[A]      [B]", 3, 3);
  color = BLACK;
  text("   Select   Down", 3, 3);
  
  color = LIGHT_BLACK;
  text("2x[B] Menu", 3, 9);
  
  // Key repeat handling
  if (input.b.isPressed || input.down.isPressed || input.up.isPressed) {
    settingsKeyRepeatTicks++;
  } else {
    settingsKeyRepeatTicks = 0;
  }
  
  // Get component count dynamically
  int componentCount = settings_getComponentCount();
  
  // Navigation
  if (input.b.isJustPressed || input.down.isJustPressed ||
      (settingsKeyRepeatTicks > KEY_REPEAT_DURATION &&
       (input.b.isPressed || input.down.isPressed))) {
    settingsIndex++;
    if (settingsKeyRepeatTicks > KEY_REPEAT_DURATION) {
      settingsKeyRepeatTicks = KEY_REPEAT_DURATION / 3 * 2;
    }
  }
  if (input.up.isJustPressed ||
      (settingsKeyRepeatTicks > KEY_REPEAT_DURATION && input.up.isPressed)) {
    settingsIndex--;
    if (settingsKeyRepeatTicks > KEY_REPEAT_DURATION) {
      settingsKeyRepeatTicks = KEY_REPEAT_DURATION / 3 * 2;
    }
  }
  settingsIndex = wrap(settingsIndex, 0, componentCount);
  
  // Draw menu items - generic component rendering
  color = BLACK;
  for (int i = 0; i < componentCount; i++) {
    float y = i * 6 + 6;
    if (i == settingsIndex) {
      color = BLUE;
      text(">", 3, y + 9);
      color = BLACK;
    }
    
    // Draw component name
    const char* name = settings_getComponentName(i);
    if (name) {
      text((char*)name, 9, y + 9);
    }
    
    // Draw component state with color
    const char* stateText = settings_getComponentStateText(i);
    int stateColor = settings_getComponentStateColor(i);
    if (stateText) {
      color = stateColor;
      text((char*)stateText, 45, y + 9);
      color = BLACK;
    }
  }
  
  // Handle selection - toggle component
  if (input.a.isJustPressed && settingsIndex < componentCount) {
    settings_toggleComponent(settingsIndex);
  }
}

// Main menu mode update function
static void updateMenuMode() {
  color = BLUE;
  text("[A]      [B]", 3, 3);
  color = BLACK;
  text("   Select   Down", 3, 3);
  
  color = LIGHT_BLACK;
  text("2x[B] Settings", 3, 9);
  
  // Double tap B for settings
  if (checkDoubleClick(&input.b)) {
    setMode(1); // Switch to settings mode
    settingsIndex = 0;
    return;
  }
  
  if (input.b.isPressed || input.down.isPressed || input.up.isPressed) {
    gameKeyRepeatTicks++;
  } else {
    gameKeyRepeatTicks = 0;
  }
  
  if (input.b.isJustPressed || input.down.isJustPressed ||
      (gameKeyRepeatTicks > KEY_REPEAT_DURATION &&
       (input.b.isPressed || input.down.isPressed))) {
    gameIndex++;
    if (gameKeyRepeatTicks > KEY_REPEAT_DURATION) {
      gameKeyRepeatTicks = KEY_REPEAT_DURATION / 3 * 2;
    }
  }
  if (input.up.isJustPressed ||
      (gameKeyRepeatTicks > KEY_REPEAT_DURATION && input.up.isPressed)) {
    gameIndex--;
    if (gameKeyRepeatTicks > KEY_REPEAT_DURATION) {
      gameKeyRepeatTicks = KEY_REPEAT_DURATION / 3 * 2;
    }
  }
  gameIndex = wrap(gameIndex, 1, gameCount);
  color = BLACK;
  for (int i = 0; i < gameCount; i++) {
    float y = i * 6 + 6;
    if (i == gameIndex) {
      color = BLUE;
      text(">", 3, y + 9);
      color = BLACK;
    }
    text(games[i].title, 9, y + 9);
  }
  if (input.a.isJustPressed) {
    restartGame(gameIndex);
  }
}

void addGame(char *title, char *description,
             char (*characters)[CHARACTER_WIDTH][CHARACTER_HEIGHT + 1],
             int charactersCount, Options options, void (*update)(void)) {
  if (gameCount >= MAX_GAME_COUNT) {
    return;
  }
  Game *g = &games[gameCount];
  g->title = title;
  g->description = description;
  g->characters = characters;
  g->charactersCount = charactersCount;
  g->options = options;
  g->update = update;
  gameCount++;
}

Game getGame(int index) { return games[index]; }

// Unified update function that delegates to current mode
static void update() {
  updateCurrentMode();
}

void addMenu() {
  // Register modes FIRST before adding game with update()
  registerMode(updateMenuMode);     // Mode 0: Main menu
  registerMode(updateSettingsMode); // Mode 1: Settings
  
  // No need to manually add settings items - they come from SettingsService components
  
  // Add the menu game (update() will call updateCurrentMode which needs modes registered)
  Options o = {
      .viewSizeX = 130, .viewSizeY = 230, .soundSeed = 0, .isDarkColor = true};
  addGame("", "", NULL, 0, o, update);
}
