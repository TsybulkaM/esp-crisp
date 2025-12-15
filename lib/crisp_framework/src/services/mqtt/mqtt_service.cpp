#include "mqtt_service.h"
#include "services/core/core_service.h"
#include <esp_timer.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <stdio.h>
#include <ctype.h>

static const char* TAG = "mqtt";
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
    isConnected_ = false;
    reconnectTask = nullptr;
    publishTask = nullptr;
    scoreQueue = xQueueCreate(10, sizeof(ScoreMessage));
    lastPublishTime = 0;
    currentGameCode[0] = '\0';
    currentState = STATE_IDLE;
    stateEnterTime = 0;
    reconnectDelayMs = 5000;
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
    
    // Start publish task and FSM
    xTaskCreatePinnedToCore(publishTaskFunc, "mqttPub", 3072, this, 5, &publishTask, 1);
    xTaskCreatePinnedToCore(state_machine_task_func, "mqttFSM", 3072, this, 4, &reconnectTask, 1);
    
    return true;
}

void MqttService::stop() {
    CoreService::log_info(TAG, "Stopping MQTT service...");
    
    // Stop FSM first
    if (reconnectTask) {
        vTaskDelete(reconnectTask);
        reconnectTask = nullptr;
    }
    
    // Stop publish task
    if (publishTask) {
        vTaskDelete(publishTask);
        publishTask = nullptr;
    }
    
    // Stop MQTT client
    if (client) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = nullptr;
    }
    
    isConnected_ = false;
    CoreService::log_info(TAG, "MQTT service stopped");
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
            CoreService::log_info(TAG, "MQTT event: connected");
            isConnected_ = true;
            if (currentState == STATE_CONNECTING) {
                changeState(STATE_CONNECTED);
            }
            break;
        }
        case MQTT_EVENT_DISCONNECTED:
            CoreService::log_warn(TAG, "MQTT event: disconnected");
            isConnected_ = false;
            if (currentState == STATE_CONNECTED) {
                changeState(STATE_DISCONNECTED);
            }
            break;
        case MQTT_EVENT_ERROR:
            CoreService::log_error(TAG, "MQTT event: error");
            isConnected_ = false;
            if (currentState == STATE_CONNECTED || currentState == STATE_CONNECTING) {
                changeState(STATE_DISCONNECTED);
            }
            break;
        default:
            break;
    }
}

bool MqttService::publishScore(int score, const char* gameCode) {
    if (!scoreQueue) return false;
    
    ScoreMessage msg;
    msg.score = score;
    
    const char* game = gameCode ? gameCode : currentGameCode;
    if (game && game[0] != '\0') {
        strncpy(msg.gameCode, game, sizeof(msg.gameCode) - 1);
        msg.gameCode[sizeof(msg.gameCode) - 1] = '\0';
    } else {
        strcpy(msg.gameCode, "UNKNOWN");
    }
    
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
        if (xQueueReceive(self->scoreQueue, &msg, portMAX_DELAY) == pdTRUE) {
            if (self->isConnected_) {
                self->doPublishScore(msg.score, msg.gameCode);
            }
        }
    }
}

void MqttService::doPublishScore(int score, const char* gameCode) {
    int64_t now = esp_timer_get_time() / 1000;
    if (now - lastPublishTime < PUBLISH_INTERVAL_MS) {
        return;
    }
    lastPublishTime = now;
    
    if (!client || !isConnected_) return;
    char topic[128];
    snprintf(topic, sizeof(topic), "devices/%s/score", deviceId);
    
    char payload[128];
    snprintf(payload, sizeof(payload), "{\"game\":\"%s\",\"score\":%d}", gameCode, score);
    
    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
    CoreService::log_info(TAG, "Published score=%d game=%s, msg_id=%d", score, gameCode, msg_id);
}


// =======================
// ===== NVS helpers =====
// =======================

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

void MqttService::state_machine_task_func(void* param) {
    MqttService* self = (MqttService*)param;
    self->changeState(STATE_IDLE);
    
    while (true) {
        self->processFSM();
        vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second
    }
}

void MqttService::changeState(State newState) {
    if (currentState != newState) {
        CoreService::log_info(TAG, "FSM: %d -> %d", currentState, newState);
        currentState = newState;
        stateEnterTime = esp_timer_get_time() / 1000; // milliseconds
    }
}

bool MqttService::isWiFiConnected() {
    wifi_ap_record_t ap_info;
    return (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
}

void MqttService::processFSM() {
    int64_t now = esp_timer_get_time() / 1000;
    int64_t timeInState = now - stateEnterTime;
    
    switch (currentState) {
        case STATE_IDLE:
            changeState(STATE_CHECK_WIFI);
            break;
            
        case STATE_CHECK_WIFI:
            if (isWiFiConnected()) {
                CoreService::log_info(TAG, "WiFi connected, attempting MQTT connect");
                changeState(STATE_CONNECTING);
            } else {
                if (timeInState > 10000) {
                    CoreService::log_warn(TAG, "Waiting for WiFi...");
                    stateEnterTime = now;
                }
            }
            break;
            
        case STATE_CONNECTING:
            if (!isWiFiConnected()) {
                CoreService::log_warn(TAG, "WiFi lost during connect");
                isConnected_ = false;
                if (client) {
                    esp_mqtt_client_stop(client);
                }
                changeState(STATE_CHECK_WIFI);
                break;
            }
            
            if (!client) {
                CoreService::log_error(TAG, "MQTT client not initialized");
                changeState(STATE_DISCONNECTED);
                break;
            }
            
            // Wait for connection event (handled by handle_event)
            if (isConnected_) {
                changeState(STATE_CONNECTED);
            } else if (timeInState > 30000) { // 30 second timeout
                CoreService::log_error(TAG, "MQTT connect timeout");
                changeState(STATE_DISCONNECTED);
            }
            break;
            
        case STATE_CONNECTED:
            if (!isWiFiConnected()) {
                CoreService::log_warn(TAG, "WiFi lost");
                isConnected_ = false;
                if (client) {
                    esp_mqtt_client_stop(client);
                }
                changeState(STATE_CHECK_WIFI);
                break;
            }
            
            if (!isConnected_) {
                CoreService::log_warn(TAG, "MQTT disconnected");
                changeState(STATE_DISCONNECTED);
                break;
            }
            break;
            
        case STATE_DISCONNECTED:
            if (!isWiFiConnected()) {
                changeState(STATE_CHECK_WIFI);
                reconnectDelayMs = 5000; // Reset delay
                break;
            }
            
            // Wait with exponential backoff, then reconnect
            if (timeInState >= reconnectDelayMs) {
                CoreService::log_info(TAG, "Attempting MQTT reconnect...");
                if (client && isWiFiConnected()) {
                    esp_err_t err = esp_mqtt_client_reconnect(client);
                    if (err == ESP_OK) {
                        changeState(STATE_CONNECTING);
                        reconnectDelayMs = 5000; // Reset on success
                    } else {
                        CoreService::log_error(TAG, "Reconnect failed: %d", err);
                        // Exponential backoff
                        reconnectDelayMs = (reconnectDelayMs * 2 > 60000) ? 60000 : reconnectDelayMs * 2;
                        stateEnterTime = now; // Restart delay
                    }
                } else {
                    changeState(STATE_CHECK_WIFI);
                }
            }
            break;
    }
}

