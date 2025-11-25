#include "mqtt_service.h"

extern "C" {
    #include "cglp.h"
}

// C linkage
extern "C" {
    void publishScore(int score, const char* gameCode) {
        auto service = MqttService::getInstance();
        if (service && gameCode) {
            // Sanitize game title to snake_case for MQTT topics
            char sanitized[32];
            MqttService::sanitizeGameCode(gameCode, sanitized, sizeof(sanitized));
            service->publishScore(score, sanitized);
        }
    }
}