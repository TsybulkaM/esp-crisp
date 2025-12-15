#include "service_manager.h"
#include "../mqtt/mqtt_service.h"
#include "../fota/fota.h"
#include "../core/core_service.h"

static const char* TAG = "service_mgr";

ServiceManager* ServiceManager::instance = nullptr;

ServiceManager* ServiceManager::getInstance() {
    return instance;
}

void ServiceManager::setInstance(ServiceManager* manager) {
    instance = manager;
}

void ServiceManager::registerMqttService(MqttService* service) {
    mqttService = service;
}

void ServiceManager::registerFotaService(FotaService* service) {
    fotaService = service;
}

void ServiceManager::requestServiceStop(const char* serviceName, ServiceStopCallback callback) {
    CoreService::log_info(TAG, "Stop requested for: %s", serviceName);
    
    if (strcmp(serviceName, "mqtt") == 0 && mqttService) {
        mqttService->stop();
        if (callback) {
            callback();
        }
    } else if (strcmp(serviceName, "fota") == 0 && fotaService) {
        fotaService->stop();
        if (callback) {
            callback();
        }
    }
}

void ServiceManager::requestServiceStart(const char* serviceName, ServiceStartCallback callback) {
    CoreService::log_info(TAG, "Start requested for: %s", serviceName);
    
    if (strcmp(serviceName, "mqtt") == 0 && mqttService) {
        mqttService->start();
        if (callback) {
            callback();
        }
    } else if (strcmp(serviceName, "fota") == 0 && fotaService) {
        fotaService->start();
        if (callback) {
            callback();
        }
    }
}

bool ServiceManager::isServiceRunning(const char* serviceName) {
    if (strcmp(serviceName, "mqtt") == 0 && mqttService) {
        return mqttService->isConnected();
    } else if (strcmp(serviceName, "fota") == 0 && fotaService) {
        return fotaService->isRunning();
    }
    return false;
}
