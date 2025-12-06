#pragma once

#include <M5Unified.h>

class CoreService
{
public:
    CoreService();

    const char* getDeviceId() const { return deviceId; }

    static CoreService* getInstance() { return instance; }
    static void setInstance(CoreService* svc) { instance = svc; }

    #if CORE_DEBUG_LEVEL
    // --- Logging utilities ---

    static void log_info(const char* tag, const char* format, ...);
    static void log_debug(const char* tag, const char* format, ...);
    static void log_warn(const char* tag, const char* format, ...);
    static void log_error(const char* tag, const char* format, ...);

    // --- End logging utilities ---
    // --- Serial command interface ---

    static void command_executor(const char* command);
    void serial_command_task(void* param);
    void create_serial_command_task();

    // NVS utility functions
    static void nvs_list_all();
    static void nvs_get_string(const char* ns, const char* key);
    static void nvs_get_int(const char* ns, const char* key);
    static void nvs_set_string(const char* ns, const char* key, const char* value);
    static void nvs_set_int(const char* ns, const char* key, int value);
    static void nvs_delete_key(const char* ns, const char* key);
    static void nvs_erase_namespace(const char* ns);
    static void nvs_erase_all();

    // --- End serial command interface ---

    // --- System monitor task ---
    void start_system_monitor();
    // --- End system monitor task ---

    #endif

private:
    static CoreService* instance;
    char deviceId[32];
};