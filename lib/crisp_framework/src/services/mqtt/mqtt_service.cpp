#include "mqtt_service.h"
#include "services/core/core_service.h"
#include <esp_timer.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <stdio.h>
#include <ctype.h>

static const char* TAG = "MqttService";
static const char* MQTT_NVS_NAMESPACE = "mqtt";
static const char* MQTT_BROKER_KEY = "broker_uri";

MqttService* MqttService::instance = nullptr;

MqttService::MqttService(const char* deviceIdArg, const char* broker_uri) {
    strncpy(deviceId, deviceIdArg, sizeof(deviceId) - 1);
    deviceId[sizeof(deviceId)-1] = '\0';
    
    // Load broker from NVS or use provided/default
    const char* savedBroker = getBrokerUri();
    if (savedBroker && savedBroker[0] != '\0') {
        strncpy(brokerUri, savedBroker, sizeof(brokerUri) - 1);
    } else {
        strncpy(brokerUri, broker_uri, sizeof(brokerUri) - 1);
    }
    brokerUri[sizeof(brokerUri)-1] = '\0';
    
    client = nullptr;
    isConnected = false;
    reconnectTask = nullptr;
    scoreQueue = xQueueCreate(10, sizeof(ScoreMessage));
    publishTask = nullptr;
    lastPublishTime = 0;
    currentGameCode[0] = '\0';
}

MqttService::~MqttService() {
    stop();
    if (reconnectTask) {
        vTaskDelete(reconnectTask);
        reconnectTask = nullptr;
    }
    if (publishTask) {
        vTaskDelete(publishTask);
        publishTask = nullptr;
    }
    if (scoreQueue) {
        vQueueDelete(scoreQueue);
        scoreQueue = nullptr;
    }
}

bool MqttService::start() {
    if (client) return true;

    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = brokerUri;
    cfg.credentials.client_id = deviceId;

    client = esp_mqtt_client_init(&cfg);
    if (!client) {
        CoreService::log_error(TAG, "Failed to init MQTT client");
        return false;
    }

    esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, MqttService::mqtt_event_handler_cb, this);

    esp_err_t err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        CoreService::log_error(TAG, "Failed to start MQTT client: %d", err);
        return false;
    }
    CoreService::log_info(TAG, "MQTT client started, broker=%s, client_id=%s", brokerUri, deviceId);
    
    // Start publish task and reconnect monitor
    xTaskCreatePinnedToCore(publishTaskFunc, "mqttPub", 4096, this, 5, &publishTask, 1);
    xTaskCreatePinnedToCore(reconnectTaskFunc, "mqttRecon", 3072, this, 4, &reconnectTask, 1);
    
    return true;
}

// Start MQTT client in background task - non-blocking
void MqttService::startAsync() {
    xTaskCreatePinnedToCore(
        [](void* param) {
            MqttService* self = (MqttService*)param;
            // Small delay to let game start first
            vTaskDelay(pdMS_TO_TICKS(2000));
            CoreService::log_info(TAG, "Starting MQTT client in background...");
            self->start();
            vTaskDelete(nullptr);
        },
        "mqttInit", 4096, this, 3, nullptr, 1
    );
}

void MqttService::stop() {
    if (client) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = nullptr;
    }
}

void MqttService::mqtt_event_handler_cb(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    if (!handler_args || !event_data) return;
    MqttService* self = (MqttService*) handler_args;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;
    self->handle_event(event);
}

void MqttService::handle_event(esp_mqtt_event_handle_t event) {
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED: {
            CoreService::log_info(TAG, "MQTT connected");
            isConnected = true;
            publishOnline();
            break;
        }
        case MQTT_EVENT_DISCONNECTED:
            CoreService::log_warn(TAG, "MQTT disconnected");
            isConnected = false;
            break;
        case MQTT_EVENT_ERROR:
            CoreService::log_error(TAG, "MQTT error");
            isConnected = false;
            break;
        default:
            break;
    }
}

bool MqttService::publishOnline() {
    if (!client) return false;
    char topic[128];
    snprintf(topic, sizeof(topic), "devices/%s/status", deviceId);
    const char* payload = "online";
    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, 1);
    CoreService::log_info(TAG, "Published online message, msg_id=%d", msg_id);
    
    return msg_id >= 0;
}

bool MqttService::publishScore(int score, const char* gameCode) {
    if (!scoreQueue || !isConnected) return false;
    
    ScoreMessage msg;
    msg.score = score;
    
    // Use provided gameCode or fall back to currentGameCode
    const char* game = gameCode ? gameCode : currentGameCode;
    if (game && game[0] != '\0') {
        strncpy(msg.gameCode, game, sizeof(msg.gameCode) - 1);
        msg.gameCode[sizeof(msg.gameCode) - 1] = '\0';
    } else {
        strcpy(msg.gameCode, "UNKNOWN");
    }
    
    // Non-blocking: just queue the message
    if (xQueueSend(scoreQueue, &msg, 0) != pdTRUE) {
        CoreService::log_warn(TAG, "Score queue full, dropping score=%d game=%s", score, msg.gameCode);
        return false;
    }
    return true;
}

void MqttService::publishTaskFunc(void* param) {
    MqttService* self = (MqttService*) param;
    ScoreMessage msg;
    while (true) {
        // Wait for score from queue
        if (xQueueReceive(self->scoreQueue, &msg, portMAX_DELAY) == pdTRUE) {
            self->doPublishScore(msg.score, msg.gameCode);
        }
    }
}

void MqttService::doPublishScore(int score, const char* gameCode) {
    // Throttle: publish max once per second
    int64_t now = esp_timer_get_time() / 1000; // ms
    if (now - lastPublishTime < PUBLISH_INTERVAL_MS) {
        return; // silently throttle
    }
    lastPublishTime = now;
    
    if (!client || !isConnected) return;
    char topic[128];
    snprintf(topic, sizeof(topic), "devices/%s/score", deviceId);
    
    // JSON payload with game code and score
    char payload[128];
    snprintf(payload, sizeof(payload), "{\"game\":\"%s\",\"score\":%d}", gameCode, score);
    
    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
    CoreService::log_info(TAG, "Published score=%d game=%s, msg_id=%d", score, gameCode, msg_id);
}

void MqttService::reconnectTaskFunc(void* param) {
    MqttService* self = (MqttService*) param;
    int reconnectDelay = 5000; // Start with 5 seconds
    const int maxReconnectDelay = 60000; // Max 60 seconds
    
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(reconnectDelay));
        
        // Check WiFi status
        wifi_ap_record_t ap_info;
        bool wifiConnected = (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
        
        if (!wifiConnected) {
            if (self->isConnected) {
                CoreService::log_warn(TAG, "WiFi disconnected, stopping MQTT");
                self->isConnected = false;
                if (self->client) {
                    esp_mqtt_client_stop(self->client);
                }
            }
            reconnectDelay = 5000; // Reset delay when WiFi is down
            continue;
        }
        
        // WiFi is up, check MQTT
        if (!self->isConnected && self->client) {
            CoreService::log_info(TAG, "Attempting MQTT reconnect...");
            esp_err_t err = esp_mqtt_client_reconnect(self->client);
            if (err == ESP_OK) {
                reconnectDelay = 5000; // Reset on successful reconnect
            } else {
                // Exponential backoff
                reconnectDelay = (reconnectDelay * 2 > maxReconnectDelay) ? maxReconnectDelay : reconnectDelay * 2;
                CoreService::log_warn(TAG, "MQTT reconnect failed, retry in %d ms", reconnectDelay);
            }
        } else if (self->isConnected) {
            reconnectDelay = 5000; // Reset when connected
        }
    }
}

// Load broker URI from NVS
const char* MqttService::getBrokerUri() {
    static char uri[128] = {0};
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(MQTT_NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t required = sizeof(uri);
        if (nvs_get_str(nvs_handle, MQTT_BROKER_KEY, uri, &required) == ESP_OK) {
            nvs_close(nvs_handle);
            return uri;
        }
        nvs_close(nvs_handle);
    }
    return nullptr;
}

// Save broker URI to NVS
bool MqttService::saveBrokerUri(const char* uri) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(MQTT_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return false;
    
    nvs_set_str(nvs_handle, MQTT_BROKER_KEY, uri);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return true;
}

// Sanitize game title to snake_case for MQTT topic
void MqttService::sanitizeGameCode(const char* title, char* outCode, size_t outSize) {
    if (!title || !outCode || outSize == 0) return;
    
    size_t j = 0;
    for (size_t i = 0; title[i] != '\0' && j < outSize - 1; i++) {
        char c = title[i];
        if (isalnum(c)) {
            outCode[j++] = tolower(c);
        } else if (c == ' ' || c == '-') {
            if (j > 0 && outCode[j-1] != '_') {
                outCode[j++] = '_';
            }
        }
    }
    // Remove trailing underscore
    if (j > 0 && outCode[j-1] == '_') {
        j--;
    }
    outCode[j] = '\0';
    
    // Fallback if empty
    if (j == 0) {
        strncpy(outCode, "unknown", outSize - 1);
        outCode[outSize - 1] = '\0';
    }
}
