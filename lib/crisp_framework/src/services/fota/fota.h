#pragma once

#include <esp_err.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

class FotaService {
public:
    FotaService(const char* updateUrl);
    ~FotaService();

    bool start();
    void stop();
    bool isRunning() const { return isRunning_; }
    
    void checkNow();  // Manual check
    bool checkForUpdate(char* outDownloadUrl, size_t urlSize);  // For testing
    
    static void setInstance(FotaService* svc) { instance = svc; }
    static FotaService* getInstance() { return instance; }
    
    // Config helpers
    static const char* getUpdateUrl();
    static bool saveUpdateUrl(const char* url);
    static const char* getCurrentVersion();
    static bool saveCurrentVersion(const char* version);

private:
    char updateUrl[256];
    
    // FSM states
    enum State {
        STATE_IDLE,
        STATE_CHECK_WIFI,
        STATE_CHECKING_UPDATE,
        STATE_DOWNLOADING,
        STATE_ERROR
    };
    
    State currentState;
    TaskHandle_t checkTask;
    int64_t lastCheckTime;
    int checkIntervalMs;
    bool isRunning_;
    
    static FotaService* instance;
    static void checkTaskFunc(void* param);
    void processFSM();
    void changeState(State newState);
    bool isWiFiConnected();
    bool downloadAndInstall(const char* downloadUrl, char* outVersion, size_t versionSize);
};
