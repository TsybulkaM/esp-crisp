#pragma once

#include <M5Unified.h>

class CoreService
{
public:
    CoreService();

    const char* getDeviceId() const { return deviceId; }

    static CoreService* getInstance() { return instance; }
    static void setInstance(CoreService* svc) { instance = svc; }

    static void log_info(const char* tag, const char* format, ...);
    static void log_debug(const char* tag, const char* format, ...);
    static void log_warn(const char* tag, const char* format, ...);
    static void log_error(const char* tag, const char* format, ...);
private:
    static CoreService* instance;
    char deviceId[32];
};