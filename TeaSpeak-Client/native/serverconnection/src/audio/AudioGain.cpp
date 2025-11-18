//
// Created by WolverinDEV on 09/08/2020.
//

#include <cmath>
#include "AudioGain.h"
#include "../logger.h"

bool tc::audio::apply_gain(void *vp_buffer, size_t channel_count, size_t sample_count, float gain) {
    if(gain == 1.f) {
        return false;
    }

    bool audio_clipped{false};
    auto buffer = (float*) vp_buffer;
    auto buffer_end = buffer + channel_count * sample_count;
    while (buffer != buffer_end) {
        auto& value = *buffer++;
        value *= gain;

        if(value > 1.f) {
            log_debug(category::audio, tr("Audio gain apply clipped: {}"), (float) value);
            value = std::isinf(value) ? 0 : 1.f;
            audio_clipped = true;
        }
    }

    return audio_clipped;
}
