#include "sound_service.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include <float.h>

SoundService* SoundService::instance = nullptr;

SoundService::SoundService(IBuzzer& buzzer, float tempo)
    : buzzerDriver(buzzer), tempo(tempo), soundToneIndex(0), soundTime(0)
{
    initSoundTones();
}

void SoundService::initSoundTones()
{
    for (int i = 0; i < SOUND_TONE_COUNT; i++)
    {
        this->soundTones[i].when = FLT_MAX;
    }
}

void SoundService::addSoundTone(float freq, float duration, float when)
{
    SoundTone *st = &soundTones[soundToneIndex];
    st->freq = freq;
    st->duration = duration;
    st->when = when;
    soundToneIndex++;
    if (soundToneIndex >= SOUND_TONE_COUNT)
    {
        soundToneIndex = 0;
    }
}

void SoundService::playTone(float freq, float duration, float when)
{
    this->addSoundTone(freq, duration, when);
}

void SoundService::stopTone()
{
    this->initSoundTones();
    buzzerDriver.stop();
}

float SoundService::getAudioTime() {
    return soundTime; 
}

void SoundService::updateFromSoundTask()
{
    soundTime += 60.0f / tempo / TONE_PER_NOTE;
    float lastWhen = 0;
    int ti = -1;
    for (int i = 0; i < SOUND_TONE_COUNT; i++)
    {
        SoundTone *st = &soundTones[i];
        if (st->when <= soundTime)
        {
            if (st->when > lastWhen)
            {
                ti = i;
                lastWhen = st->when;
                st->when = FLT_MAX;
            }
        }
    }
    if (ti >= 0)
    {
        SoundTone *st = &soundTones[ti];
        buzzerDriver.tone((uint16_t)st->freq, (uint32_t)(st->duration * 1000));
    }
}

void SoundService::updateSoundTaskStatic(void *pvParameters)
{
    SoundService* service = static_cast<SoundService*>(pvParameters);
    while (1)
    {
        xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
        service->updateFromSoundTask();
    }
}

void SoundService::soundTimerCallbackStatic(TimerHandle_t xTimer)
{
    SoundService* service = static_cast<SoundService*>(pvTimerGetTimerID(xTimer));
    xTaskNotifyFromISR(service->soundTaskHandle, 0, eIncrement, NULL);
}

void SoundService::createTask()
{
    xTaskCreate(updateSoundTaskStatic, "updateSoundTask", 8192, this, 2, &soundTaskHandle);

    float soundFreq = (tempo / 60.0f) * TONE_PER_NOTE;
    uint32_t sound_interval_ms = (uint32_t)(1000.0f / soundFreq);

    soundTimer = xTimerCreate("soundTimer", pdMS_TO_TICKS(sound_interval_ms),
                            pdTRUE, this, soundTimerCallbackStatic);

    xTimerStart(soundTimer, 0);
}