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
    MqttService(const char* deviceId, const char* broker_uri = "mqtt://broker.hivemq.com");
    ~MqttService();

    bool start();
    void startAsync(); // Non-blocking background start
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

    // Connection state
    bool isConnected;
    TaskHandle_t reconnectTask;
    
    // Throttling & async publishing
    QueueHandle_t scoreQueue;
    TaskHandle_t publishTask;
    int64_t lastPublishTime;
    static const int PUBLISH_INTERVAL_MS = 1000; // 1 second

    static MqttService* instance;
    static void mqtt_event_handler_cb(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
    void handle_event(esp_mqtt_event_handle_t event);
    static void publishTaskFunc(void* param);
    static void reconnectTaskFunc(void* param);
    void doPublishScore(int score, const char* gameCode);
    
    // Config helpers
    static const char* getBrokerUri();
    static bool saveBrokerUri(const char* uri);
};
