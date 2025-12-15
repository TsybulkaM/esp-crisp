#include <unity.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include "../lib/crisp_framework/src/services/fota/fota.h"
#include "../lib/crisp_framework/src/services/mqtt/mqtt_service.h"
#include "../lib/crisp_framework/src/services/interfaces/service_manager.h"
#include "../lib/crisp_framework/src/services/core/core_service.h"

// Add missing macro
#define TEST_ASSERT_NOT_EQUAL_STRING(expected, actual) \
    TEST_ASSERT_TRUE_MESSAGE(strcmp(expected, actual) != 0, "Strings should not be equal")

// Global test fixtures
static ServiceManager* serviceManager = nullptr;

void setUp(void) {
    // Initialize service manager for each test
    if (!serviceManager) {
        serviceManager = new ServiceManager();
        ServiceManager::setInstance(serviceManager);
    }
}

void tearDown(void) {
    // Cleanup after each test
}

// ============================================================================
// FOTA Service Tests
// ============================================================================

void test_fota_initialization(void) {
    FotaService fota("http://test.com/api/fota");
    TEST_ASSERT_TRUE(fota.isRunning() == false);
}

void test_fota_start_stop(void) {
    FotaService fota("http://test.com/api/fota");
    
    bool started = fota.start();
    TEST_ASSERT_TRUE(started);
    TEST_ASSERT_TRUE(fota.isRunning());
    
    fota.stop();
    TEST_ASSERT_FALSE(fota.isRunning());
}

void test_fota_version_persistence(void) {
    // Save original version
    const char* originalVersion = FotaService::getCurrentVersion();
    char savedOriginal[32] = {0};
    if (originalVersion && strlen(originalVersion) > 0) {
        strncpy(savedOriginal, originalVersion, sizeof(savedOriginal) - 1);
    }
    
    // Test that version can be saved and retrieved
    const char* testVersion = "9.9.9-test";
    
    bool saved = FotaService::saveCurrentVersion(testVersion);
    TEST_ASSERT_TRUE(saved);
    
    const char* retrieved = FotaService::getCurrentVersion();
    TEST_ASSERT_NOT_NULL(retrieved);
    TEST_ASSERT_EQUAL_STRING(testVersion, retrieved);
    
    // Restore original version
    if (strlen(savedOriginal) > 0) {
        FotaService::saveCurrentVersion(savedOriginal);
    }
}

void test_fota_url_persistence(void) {
    // Save original URL
    const char* originalUrl = FotaService::getUpdateUrl();
    char savedOriginal[256] = {0};
    if (originalUrl && strlen(originalUrl) > 0) {
        strncpy(savedOriginal, originalUrl, sizeof(savedOriginal) - 1);
    }
    
    const char* testUrl = "http://test-server.local/api/fota/test";
    
    bool saved = FotaService::saveUpdateUrl(testUrl);
    TEST_ASSERT_TRUE(saved);
    
    const char* retrieved = FotaService::getUpdateUrl();
    TEST_ASSERT_NOT_NULL(retrieved);
    TEST_ASSERT_EQUAL_STRING(testUrl, retrieved);
    
    // Restore original URL
    if (strlen(savedOriginal) > 0) {
        FotaService::saveUpdateUrl(savedOriginal);
    }
}

// ============================================================================
// MQTT Service Tests
// ============================================================================

void test_mqtt_initialization(void) {
    MqttService mqtt("test_device");
    TEST_ASSERT_FALSE(mqtt.isConnected());
}

void test_mqtt_start_creates_tasks(void) {
    MqttService mqtt("test_device");
    serviceManager->registerMqttService(&mqtt);
    
    bool started = mqtt.start();
    TEST_ASSERT_TRUE(started);
    
    // Give time for tasks to initialize
    vTaskDelay(pdMS_TO_TICKS(100));
    
    mqtt.stop();
}

void test_mqtt_stop_cleans_tasks(void) {
    MqttService mqtt("test_device");
    serviceManager->registerMqttService(&mqtt);
    
    mqtt.start();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    mqtt.stop();
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // After stop, should not be connected
    TEST_ASSERT_FALSE(mqtt.isConnected());
}

void test_mqtt_broker_url_persistence(void) {
    // Save original broker
    const char* originalBroker = MqttService::getBrokerUri();
    char savedOriginal[256] = {0};
    if (originalBroker && strlen(originalBroker) > 0) {
        strncpy(savedOriginal, originalBroker, sizeof(savedOriginal) - 1);
    }
    
    const char* testBroker = "mqtt://test-broker.local:1883";
    
    bool saved = MqttService::saveBrokerUri(testBroker);
    TEST_ASSERT_TRUE(saved);
    
    const char* retrieved = MqttService::getBrokerUri();
    TEST_ASSERT_NOT_NULL(retrieved);
    TEST_ASSERT_EQUAL_STRING(testBroker, retrieved);
    
    // Restore original broker
    if (strlen(savedOriginal) > 0) {
        MqttService::saveBrokerUri(savedOriginal);
    }
}

// ============================================================================
// Integration Tests - Service Coordination
// ============================================================================

void test_service_manager_mqtt_coordination(void) {
    MqttService mqtt("test_device");
    serviceManager->registerMqttService(&mqtt);
    
    mqtt.start();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Stop through service manager
    serviceManager->requestServiceStop("mqtt", nullptr);
    vTaskDelay(pdMS_TO_TICKS(300));
    
    TEST_ASSERT_FALSE(mqtt.isConnected());
}

void test_fota_and_mqtt_coexistence(void) {
    // Verify both services can be initialized together
    FotaService fota("http://test.com/api/fota");
    MqttService mqtt("test_device");
    
    serviceManager->registerFotaService(&fota);
    serviceManager->registerMqttService(&mqtt);
    
    TEST_ASSERT_TRUE(fota.start());
    TEST_ASSERT_TRUE(mqtt.start());
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    fota.stop();
    mqtt.stop();
    
    vTaskDelay(pdMS_TO_TICKS(300));
    
    TEST_ASSERT_FALSE(fota.isRunning());
    TEST_ASSERT_FALSE(mqtt.isConnected());
}

// ============================================================================
// Test Runner
// ============================================================================

extern "C" void app_main() {
    // Initialize NVS for persistence tests
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize core service for logging
    auto core_service = new CoreService();
    CoreService::setInstance(core_service);
    
    vTaskDelay(pdMS_TO_TICKS(2000)); // Wait for system to stabilize
    
    UNITY_BEGIN();
    
    // FOTA Tests
    RUN_TEST(test_fota_initialization);
    RUN_TEST(test_fota_start_stop);
    RUN_TEST(test_fota_version_persistence);
    RUN_TEST(test_fota_url_persistence);
    
    // MQTT Tests
    RUN_TEST(test_mqtt_initialization);
    RUN_TEST(test_mqtt_start_creates_tasks);
    RUN_TEST(test_mqtt_stop_cleans_tasks);
    RUN_TEST(test_mqtt_broker_url_persistence);
    
    // Integration Tests
    RUN_TEST(test_service_manager_mqtt_coordination);
    RUN_TEST(test_fota_and_mqtt_coexistence);
    
    UNITY_END();
    
    // Cleanup
    if (serviceManager) {
        delete serviceManager;
        serviceManager = nullptr;
    }
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
