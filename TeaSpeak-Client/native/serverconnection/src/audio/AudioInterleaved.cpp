//
// Created by WolverinDEV on 27/03/2021.
//

#include <cassert>
#include <cstring>
#include "AudioInterleaved.h"

using namespace tc;

constexpr static auto kMaxChannelCount{32};
void audio::deinterleave(float *target, const float *source, size_t channel_count, size_t sample_count) {
    assert(channel_count <= kMaxChannelCount);
    assert(source != target);

    float* target_ptr[kMaxChannelCount];
    for(size_t channel{0}; channel < channel_count; channel++) {
        target_ptr[channel] = target + (channel * sample_count);
    }

    for(size_t sample{0}; sample < sample_count; sample++) {
        for(size_t channel{0}; channel < channel_count; channel++) {
            *target_ptr[channel]++ = *source++;
        }
    }
}

void audio::interleave(float *target, const float *source, size_t channel_count, size_t sample_count) {
    assert(channel_count <= kMaxChannelCount);
    assert(source != target);

    const float* source_ptr[kMaxChannelCount];
    for(size_t channel{0}; channel < channel_count; channel++) {
        source_ptr[channel] = source + (channel * sample_count);
    }

    audio::interleave_vec(target, source_ptr, channel_count, sample_count);
}

void audio::interleave_vec(float *target, const float *const *source, size_t channel_count, size_t sample_count) {
    assert(channel_count <= kMaxChannelCount);

    const float* source_ptr[kMaxChannelCount];
    memcpy(source_ptr, source, channel_count * sizeof(float*));

    for(size_t sample{0}; sample < sample_count; sample++) {
        for(size_t channel{0}; channel < channel_count; channel++) {
            *target++ = *source_ptr[channel]++;
        }
    }
}