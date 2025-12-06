#include "drivers/audio/M5/m5_audio_driver.h"
#include "drivers/frame/M5/m5_frame_driver.h"
#include "drivers/controller/M5/m5_controller_driver.h"

#include "services/core/core_service.h"
#include "services/frame/frame_service.h"
#include "services/sound/sound_service.h"
#include "services/settings/settings_service.h"
#include "services/mqtt/mqtt_service.h"

extern "C" {
    #include "cglp.h"
}

extern "C" void app_main()
{
    auto core_service = new CoreService();
    CoreService::setInstance(core_service);
    
    auto buzzer_driver = new M5BuzzerDriver();
    auto display_driver = new M5FrameDriver();
    auto controller_driver = new M5ControllerDriver();
    auto wifi_driver = new ESPWiFiDriver();

    SoundService* sound_service = new SoundService(*buzzer_driver, tempo);
    SoundService::setInstance(sound_service);

    FrameService* frame_service = new FrameService(*display_driver, *controller_driver, FPS);
    FrameService::setInstance(frame_service);
    
    SettingsService* settings_service = new SettingsService(*wifi_driver);
    SettingsService::setInstance(settings_service);

    disableSound();
    initGame();

    sound_service->createTask();
    frame_service->createTask();

    #if CORE_DEBUG_LEVEL
    core_service->create_serial_command_task();
    #endif

    #if SYS_MEM_MONITORING
    core_service->start_system_monitor();
    #endif

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
