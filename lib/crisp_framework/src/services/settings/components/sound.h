#include "settings_component.h"

class SoundSettingsComponent : public ISettingsComponent {
protected:
    void on_state_changed(unsigned int current_state) override;
    void set_default_state() override;
    
    const char* get_state_text(unsigned int state) const override;
    int get_state_color(unsigned int state) const override;
    
public:
    SoundSettingsComponent() : ISettingsComponent(1, "Sound") {}
};