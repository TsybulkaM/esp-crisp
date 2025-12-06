#include "settings_component.h"

#include "hal/wifi_hal.hpp"
#include <esp_http_server.h>
#include "esp_event.h"


ESP_EVENT_DECLARE_BASE(WIFI_CONFIG_EVENT);

enum {
    WIFI_CONFIG_EVENT_UPDATED
};

// WiFi states: 0 = OFF, 1 = AP Mode (provisioning), 2 = Connected
class WifiSettingsComponent : public ISettingsComponent {
private:
    IWiFi& wifiDriver;
    
    // HTTP server for provisioning
    httpd_handle_t httpServer;
    bool provisioningActive;
    
    // HTTP handlers
    static esp_err_t handleRoot(httpd_req_t *req);
    static esp_err_t handleSave(httpd_req_t *req);
    
    bool startHTTPServer();
    void stopHTTPServer();
    
    // Private methods for internal use
    bool startWiFiProvisioning();
    void stopWiFiProvisioning();
    bool connectToWiFi();
    
protected:
    void on_state_changed(unsigned int current_state) override;
    void set_default_state() override;
    
    const char* get_state_text(unsigned int state) const override;
    int get_state_color(unsigned int state) const override;
    
public:
    WifiSettingsComponent(IWiFi& wifi);
    ~WifiSettingsComponent();

    bool isWiFiProvisioningActive() const { return provisioningActive; }
    int getConnectedClients();
    
    // WiFi credentials management
    bool hasWiFiCredentials();
    bool saveWiFiCredentials(const char* ssid, const char* password);
    bool isWiFiConnected() const;
    const char* getWiFiIP() const;

    static void onWifiConfigUpdated(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data);
};
