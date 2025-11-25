#include "menu.h"

#include "cglp.h"

#define MAX_GAME_COUNT 16
#define MAX_MENU_ITEMS 16
#define KEY_REPEAT_DURATION 30

// External settings functions
extern void settings_toggleSound();
extern bool settings_isSoundEnabled();
extern void settings_startWiFiProvisioning();
extern void settings_stopWiFiProvisioning();
extern bool settings_isWiFiProvisioningActive();
extern bool settings_isWiFiConnected();
extern const char* settings_getWiFiIP();

// Menu item structure
typedef struct {
  char* title;
  void (*onSelect)(void);
  void (*onUpdate)(int index); // Takes index parameter for Y position
} MenuItem;

// Game menu
int gameCount = 0;
static Game games[MAX_GAME_COUNT];
static int gameIndex = 1;
static int gameKeyRepeatTicks = 0;

// Settings menu
static int settingsItemCount = 0;
static MenuItem settingsItems[MAX_MENU_ITEMS];
static int settingsIndex = 0;
static int settingsKeyRepeatTicks = 0;
static bool showWiFiDetails = false;

// Display functions for settings items (draw status inline)
static void displaySound(int index) {
  color = settings_isSoundEnabled() ? GREEN : RED;
  text(settings_isSoundEnabled() ? "ON" : "OFF", 45, (index * 6) + 15);
  color = BLACK;
}

static void displayWiFi(int index) {
  if (settings_isWiFiProvisioningActive()) {
    color = YELLOW;
    text("AP", 45, (index * 6) + 15);
  } else if (settings_isWiFiConnected()) {
    color = GREEN;
    text("ON", 45, (index * 6) + 15);
  } else {
    color = RED;
    text("OFF", 45, (index * 6) + 15);
  }
  color = BLACK;
}

// WiFi details page
static void updateWiFiDetails() {
  color = BLUE;
  text("WiFi INFO", 3, 3);
  
  if (settings_isWiFiConnected()) {
    color = GREEN;
    text("Connected", 3, 15);
    color = BLACK;
    text("IP:", 3, 21);
    text((char*)settings_getWiFiIP(), 3, 27);
  } else if (settings_isWiFiProvisioningActive()) {
    color = YELLOW;
    text("AP Mode Active", 3, 15);
    color = BLACK;
    text("SSID: M5StickC-Setup", 3, 21);
    text("IP: 192.168.4.1", 3, 27);
    color = LIGHT_BLACK;
    text("Connect to AP", 3, 39);
    text("and browse to IP", 3, 45);
  } else {
    color = RED;
    text("Not Connected", 3, 15);
    color = LIGHT_BLACK;
    text("[A] Start AP", 3, 27);
  }
  
  color = LIGHT_BLACK;
  text("[B] Back", 3, 57);
  
  // Handle actions
  if (input.a.isJustPressed) {
    if (!settings_isWiFiConnected() && !settings_isWiFiProvisioningActive()) {
      settings_startWiFiProvisioning();
    } else if (settings_isWiFiProvisioningActive()) {
      settings_stopWiFiProvisioning();
    }
  }
  
  if (input.b.isJustPressed) {
    showWiFiDetails = false;
  }
}

// Settings mode update function
static void updateSettingsMode() {
  // Show WiFi details page if active
  if (showWiFiDetails) {
    updateWiFiDetails();
    return;
  }
  
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
  settingsIndex = wrap(settingsIndex, 0, settingsItemCount);
  
  // Draw menu items
  color = BLACK;
  for (int i = 0; i < settingsItemCount; i++) {
    float y = i * 6 + 6;
    if (i == settingsIndex) {
      color = BLUE;
      text(">", 3, y + 9);
      color = BLACK;
    }
    
    // Draw item title
    text(settingsItems[i].title, 9, y + 9);
    
    // Draw item value/status
    if (settingsItems[i].onUpdate != NULL) {
      settingsItems[i].onUpdate(i); // Pass index for Y position calculation
    }
  }
  
  // Handle selection
  if (input.a.isJustPressed && settingsIndex < settingsItemCount) {
    if (settingsItems[settingsIndex].onSelect != NULL) {
      settingsItems[settingsIndex].onSelect();
    }
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

// Add item to settings menu
static void addSettingsItem(char* title, void (*onSelect)(void), void (*onUpdate)(int)) {
  if (settingsItemCount >= MAX_MENU_ITEMS) {
    return;
  }
  MenuItem *item = &settingsItems[settingsItemCount];
  item->title = title;
  item->onSelect = onSelect;
  item->onUpdate = onUpdate;
  settingsItemCount++;
}

// Callbacks for settings items
static void onSelectSound() {
  settings_toggleSound();
}

static void onSelectWiFi() {
  showWiFiDetails = true;
}

// Unified update function that delegates to current mode
static void update() {
  updateCurrentMode();
}

void addMenu() {
  // Register modes FIRST before adding game with update()
  registerMode(updateMenuMode);     // Mode 0: Main menu
  registerMode(updateSettingsMode); // Mode 1: Settings
  
  // Add settings menu items
  addSettingsItem("Sound", onSelectSound, displaySound);
  addSettingsItem("WiFi", onSelectWiFi, displayWiFi);
  
  // Then add the menu game (update() will call updateCurrentMode which needs modes registered)
  Options o = {
      .viewSizeX = 130, .viewSizeY = 230, .soundSeed = 0, .isDarkColor = true};
  addGame("", "", NULL, 0, o, update);
}
