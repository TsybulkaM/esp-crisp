#include "sound_service.h"

// C linkage
extern "C" {
    void md_playTone(float freq, float duration, float when) {
        auto svc = SoundService::getInstance();
        if (svc) {
            svc->playTone(freq, duration, when);
        }
    }

    void md_stopTone() {
        auto svc = SoundService::getInstance();
        if (svc) {
            svc->stopTone();
        }
    }

    float md_getAudioTime() {
        auto svc = SoundService::getInstance();
        if (svc) {
            return svc->getAudioTime();
        }
        return 0.0f;
    }
}
