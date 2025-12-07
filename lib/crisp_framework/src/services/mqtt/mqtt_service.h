#pragma once

#include <esp_err.h>
#include <mqtt_client.h>
#include <string.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

class MqttService {
public:
    MqttService(const char* deviceId, const char* broker_uri = "mqtt://192.168.1.179:1883");
    ~MqttService();

    bool start();
    void stop();

    bool publishOnline();
    bool publishScore(int score, const char* gameCode = nullptr);

    static void setInstance(MqttService* svc) { instance = svc; }
    static MqttService* getInstance() { return instance; }
    
    // Helper: sanitize game title to snake_case for MQTT topics
    static void sanitizeGameCode(const char* title, char* outCode, size_t outSize);

private:
    struct ScoreMessage {
        int score;
        char gameCode[32];
    };

    esp_mqtt_client_handle_t client;
    char deviceId[64];
    char brokerUri[128];
    char currentGameCode[32];

    // FSM states
    enum State {
        STATE_IDLE,
        STATE_CHECK_WIFI,
        STATE_CONNECTING,
        STATE_CONNECTED,
        STATE_DISCONNECTED
    };
    
    // Connection state
    bool isConnected;
    TaskHandle_t reconnectTask;
    TaskHandle_t publishTask;
    State currentState;
    int64_t stateEnterTime;
    int reconnectDelayMs;
    
    // Score queue for async publishing
    QueueHandle_t scoreQueue;
    int64_t lastPublishTime;
    static const int PUBLISH_INTERVAL_MS = 1000; // 1 second

    static MqttService* instance;
    static void mqtt_event_handler_cb(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
    void handle_event(esp_mqtt_event_handle_t event);
    static void state_machine_task_func(void* param);
    static void publishTaskFunc(void* param);
    void doPublishScore(int score, const char* gameCode);
    
    // FSM helpers
    void changeState(State newState);
    bool isWiFiConnected();
    void processFSM();
    
    // Config helpers
    static const char* getBrokerUri();
    static bool saveBrokerUri(const char* uri);
};
