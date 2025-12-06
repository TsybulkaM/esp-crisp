#include "core_service.h"

#include <esp_system.h>
#include <esp_mac.h>
#include <esp_log.h>
#include "esp_task_wdt.h"

#include <nvs_flash.h>
#include <nvs.h>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "esp_vfs_dev.h"
#include <driver/uart.h>


CoreService* CoreService::instance = nullptr;

CoreService::CoreService()
{
    const char* NVS_NS = "device";
    const char* NVS_KEY = "id";
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t required = sizeof(deviceId);
        if (nvs_get_str(nvs_handle, NVS_KEY, deviceId, &required) == ESP_OK) {
            nvs_close(nvs_handle);
        } else {
            nvs_close(nvs_handle);
            uint8_t mac[6];
            esp_read_mac(mac, ESP_MAC_WIFI_STA);
            snprintf(deviceId, sizeof(deviceId), "%02X%02X%02X", mac[3], mac[4], mac[5]);
            if (nvs_open(NVS_NS, NVS_READWRITE, &nvs_handle) == ESP_OK) {
                nvs_set_str(nvs_handle, NVS_KEY, deviceId);
                nvs_commit(nvs_handle);
                nvs_close(nvs_handle);
            }
        }
    } else {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(deviceId, sizeof(deviceId), "%02X%02X%02X", mac[3], mac[4], mac[5]);
        if (nvs_open(NVS_NS, NVS_READWRITE, &nvs_handle) == ESP_OK) {
            nvs_set_str(nvs_handle, NVS_KEY, deviceId);
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
        }
    }

    esp_task_wdt_deinit();
}

// --- Logging utilities implementation ---

#ifdef CORE_DEBUG_LEVEL

void CoreService::log_info(const char* tag, const char* format, ...)
{
    static char buffer[256]; 
    
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    ESP_LOGI(tag, "%s", buffer);
}

void CoreService::log_debug(const char* tag, const char* format, ...)
{
    static char buffer[256]; 
    
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    ESP_LOGD(tag, "%s", buffer);
}

void CoreService::log_warn(const char* tag, const char* format, ...)
{
    static char buffer[256]; 
    
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    ESP_LOGW(tag, "%s", buffer);
}

void CoreService::log_error(const char* tag, const char* format, ...)
{
    static char buffer[256]; 
    
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    ESP_LOGE(tag, "%s", buffer);
}

void CoreService::command_executor(const char* command) {
    const char* TAG = "NVS_CMD";
    if (!command || strlen(command) == 0) {
        return;
    }
    
    // Make a copy since we'll modify it
    char cmd_copy[256];
    strncpy(cmd_copy, command, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';
    
    // Simple tokenizer
    char* token = strtok(cmd_copy, " \t\n\r");
    if (!token) return;
    
    // Check for "nvs" command
    if (strcmp(token, "nvs") == 0) {
        token = strtok(NULL, " \t\n\r");
        if (!token) {
            CoreService::log_info(TAG, "Usage: nvs <command> [args]");
            CoreService::log_info(TAG, "Commands: list, get, get-int, set, set-int, del, erase, reset");
            return;
        }
        
        // nvs list
        if (strcmp(token, "list") == 0) {
            nvs_list_all();
        }
        // nvs get <ns> <key>
        else if (strcmp(token, "get") == 0) {
            char* ns = strtok(NULL, " \t\n\r");
            char* key = strtok(NULL, " \t\n\r");
            if (ns && key) {
                nvs_get_string(ns, key);
            } else {
                CoreService::log_error(TAG, "Usage: nvs get <namespace> <key>");
            }
        }
        // nvs get-int <ns> <key>
        else if (strcmp(token, "get-int") == 0) {
            char* ns = strtok(NULL, " \t\n\r");
            char* key = strtok(NULL, " \t\n\r");
            if (ns && key) {
                nvs_get_int(ns, key);
            } else {
                CoreService::log_error(TAG, "Usage: nvs get-int <namespace> <key>");
            }
        }
        // nvs set <ns> <key> <value>
        else if (strcmp(token, "set") == 0) {
            char* ns = strtok(NULL, " \t\n\r");
            char* key = strtok(NULL, " \t\n\r");
            char* value = strtok(NULL, "\n\r");  // Get rest of line
            if (ns && key && value) {
                // Trim leading spaces from value
                while (*value == ' ' || *value == '\t') value++;
                // Remove quotes if present
                if (value[0] == '"') {
                    value++;
                    char* end = strchr(value, '"');
                    if (end) *end = '\0';
                }
                nvs_set_string(ns, key, value);
            } else {
                CoreService::log_error(TAG, "Usage: nvs set <namespace> <key> <value>");
            }
        }
        // nvs set-int <ns> <key> <number>
        else if (strcmp(token, "set-int") == 0) {
            char* ns = strtok(NULL, " \t\n\r");
            char* key = strtok(NULL, " \t\n\r");
            char* val_str = strtok(NULL, " \t\n\r");
            if (ns && key && val_str) {
                int value = atoi(val_str);
                nvs_set_int(ns, key, value);
            } else {
                CoreService::log_error(TAG, "Usage: nvs set-int <namespace> <key> <number>");
            }
        }
        // nvs del <ns> <key>
        else if (strcmp(token, "del") == 0 || strcmp(token, "delete") == 0) {
            char* ns = strtok(NULL, " \t\n\r");
            char* key = strtok(NULL, " \t\n\r");
            if (ns && key) {
                nvs_delete_key(ns, key);
            } else {
                CoreService::log_error(TAG, "Usage: nvs del <namespace> <key>");
            }
        }
        // nvs erase <ns>
        else if (strcmp(token, "erase") == 0) {
            char* ns = strtok(NULL, " \t\n\r");
            if (ns) {
                CoreService::log_warn(TAG, "Erasing namespace '%s'...", ns);
                nvs_erase_namespace(ns);
            } else {
                CoreService::log_error(TAG, "Usage: nvs erase <namespace>");
            }
        }
        // nvs reset
        else if (strcmp(token, "reset") == 0) {
            CoreService::log_warn(TAG, "Factory reset - device will restart!");
            nvs_erase_all();
        }
        // nvs help
        else if (strcmp(token, "help") == 0) {
            CoreService::log_info(TAG, "=== NVS Commands ===");
            CoreService::log_info(TAG, "nvs list                      - List all NVS contents");
            CoreService::log_info(TAG, "nvs get <ns> <key>            - Get string value");
            CoreService::log_info(TAG, "nvs get-int <ns> <key>        - Get integer value");
            CoreService::log_info(TAG, "nvs set <ns> <key> <value>    - Set string value");
            CoreService::log_info(TAG, "nvs set-int <ns> <key> <num>  - Set integer value");
            CoreService::log_info(TAG, "nvs del <ns> <key>            - Delete key");
            CoreService::log_info(TAG, "nvs erase <ns>                - Erase namespace");
            CoreService::log_info(TAG, "nvs reset                     - Factory reset");
            CoreService::log_info(TAG, "");
            CoreService::log_info(TAG, "Examples:");
            CoreService::log_info(TAG, "  nvs list");
            CoreService::log_info(TAG, "  nvs get wifi ssid");
            CoreService::log_info(TAG, "  nvs set wifi ssid MyNetwork");
            CoreService::log_info(TAG, "  nvs set-int settings sound_enabled 1");
        }
        else {
            CoreService::log_error(TAG, "Unknown command: %s", token);
            CoreService::log_info(TAG, "Type 'nvs help' for available commands");
        }
    }
    else if (strcmp(token, "help") == 0) {
        CoreService::log_info(TAG, "Available commands:");
        CoreService::log_info(TAG, "  nvs <command>  - NVS management commands");
        CoreService::log_info(TAG, "Type 'nvs help' for NVS command details");
    }
}

static void serial_command_task_impl(void* param) {
    char cmd_buffer[256];
    int cmd_pos = 0;

    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);
    esp_vfs_dev_uart_use_driver(UART_NUM_0);
    
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    esp_vfs_dev_uart_port_set_rx_line_endings(UART_NUM_0, ESP_LINE_ENDINGS_CR);
    esp_vfs_dev_uart_port_set_tx_line_endings(UART_NUM_0, ESP_LINE_ENDINGS_CRLF);

    CoreService::log_info("SerialCmd", "CLI ready on USB (stdin). Type 'help'.");
    
    while (1) {
        int c = fgetc(stdin);
        
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        char ch = (char)c;

        if (ch == '\n' || ch == '\r') {
            printf("\r\n");
            
            if (cmd_pos > 0) {
                cmd_buffer[cmd_pos] = '\0';
                
                printf("\nExecuting: '%s'\n", cmd_buffer);
                CoreService::command_executor(cmd_buffer);
                
                cmd_pos = 0;
            }

        }
        else if (ch == '\b' || ch == 127) {
            if (cmd_pos > 0) {
                cmd_pos--;
                printf("\b \b");
            }
        }
        else if (ch >= 32 && ch < 127) {
            if (cmd_pos < (int)(sizeof(cmd_buffer) - 1)) {
                cmd_buffer[cmd_pos++] = ch;
            }
        }
    }
}

void CoreService::create_serial_command_task() {
    xTaskCreatePinnedToCore(
        serial_command_task_impl,
        "serial_cmd",
        5120,
        nullptr,
        1,
        nullptr,
        1
    );
    CoreService::log_info("CoreService", "Serial command task created");
}

#endif