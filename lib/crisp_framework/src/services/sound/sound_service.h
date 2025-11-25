#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#include "hal/audio_hal.hpp"

#define TONE_PER_NOTE 32
#define SOUND_TONE_COUNT 64

typedef struct {
    float freq;
    float duration;
    float when;
} SoundTone;

class SoundService {
public:
    SoundService(IBuzzer& buzzer, float tempo);

    static SoundService* getInstance() { return instance; }
    static void setInstance(SoundService* svc) { instance = svc; }

    void playTone(float freq, float duration, float when);
    void stopTone();
    float getAudioTime();

    void createTask();
    
private:
    static SoundService* instance;
    IBuzzer& buzzerDriver;
    float tempo;

    TaskHandle_t soundTaskHandle;
    TimerHandle_t soundTimer;

    SoundTone soundTones[SOUND_TONE_COUNT];
    int soundToneIndex;
    float soundTime;

    void initSoundTones();
    void addSoundTone(float freq, float duration, float when);

    void updateFromSoundTask();
    static void updateSoundTaskStatic(void *pvParameters);
    static void soundTimerCallbackStatic(TimerHandle_t xTimer);
};
