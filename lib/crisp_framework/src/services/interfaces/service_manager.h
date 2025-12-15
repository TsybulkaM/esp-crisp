#pragma once

#include <functional>

// Forward declarations
class MqttService;
class FotaService;

// Service lifecycle callbacks
using ServiceStopCallback = std::function<void()>;
using ServiceStartCallback = std::function<void()>;

/**
 * Interface for managing service dependencies and lifecycle
 * This allows services to coordinate without direct coupling
 */
class IServiceManager {
public:
    virtual ~IServiceManager() = default;
    
    // Service coordination
    virtual void requestServiceStop(const char* serviceName, ServiceStopCallback callback) = 0;
    virtual void requestServiceStart(const char* serviceName, ServiceStartCallback callback) = 0;
    
    // Check if service is available and running
    virtual bool isServiceRunning(const char* serviceName) = 0;
};

/**
 * Default implementation of service manager
 */
class ServiceManager : public IServiceManager {
public:
    static ServiceManager* getInstance();
    static void setInstance(ServiceManager* manager);
    
    void requestServiceStop(const char* serviceName, ServiceStopCallback callback) override;
    void requestServiceStart(const char* serviceName, ServiceStartCallback callback) override;
    bool isServiceRunning(const char* serviceName) override;
    
    // Register services
    virtual void registerMqttService(MqttService* service);
    virtual void registerFotaService(FotaService* service);
    
protected:
    static ServiceManager* instance;
    MqttService* mqttService = nullptr;
    FotaService* fotaService = nullptr;
};
