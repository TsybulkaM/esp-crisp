#pragma once

class ISettingsComponent {
private:
    const char* name;
    const unsigned int max_state;
    unsigned int state;
protected:
    const char* NVS_NAMESPACE = "settings";
    
    virtual void on_state_changed(unsigned int current_state) = 0;
    
    // Override these for custom display
    virtual const char* get_state_text(unsigned int state) const { return ""; }
    virtual int get_state_color(unsigned int state) const { return 0; } // BLACK by default
    
public:
    ISettingsComponent(unsigned int max_states, const char* component_name) 
        : name(component_name), max_state(max_states), state(0) {
        // No throw - exceptions disabled
    }
    virtual ~ISettingsComponent() = default;

    const char* get_name() const { return name; }
    
    // Initialize component to default state
    virtual void set_default_state() = 0;

    int get_state() const { return state; }

    void set_state(unsigned int new_state) {
        if (new_state > max_state) {
            state = max_state;
        } else {
            state = new_state;
        }
        on_state_changed(state);
    };

    void toggle_state() { 
        state = (state + 1) % (max_state + 1); 
        on_state_changed(state);
    };
    
    // Display methods for menu rendering
    const char* get_current_state_text() const { return get_state_text(state); }
    int get_current_state_color() const { return get_state_color(state); }
};