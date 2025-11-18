//
// Created by WolverinDEV on 28/03/2021.
//

#include <cmath>
#include <algorithm>
#include <cstring>
#include "AudioVolume.h"

using namespace tc;

/* technique based on http://www.vttoth.com/CMS/index.php/technical-notes/68 */
inline float merge_ab(float a, float b, float n) {
    /*
     * Form: A,B := [0;n]
     *       IF A <= n/2 AND B <= N/2
     *          Z = (2 * A * B) / n
     *       ELSE
     *          Z = 2(A + B) - (2/n) * A * B - n
     *
     * For a range from 0 to 2: Z = 2(A + B) - AB - 2
     */

    auto half_n = n / 2;
    auto inv_half_n = 1 / half_n;

    if(a < half_n && b < half_n) {
        return inv_half_n * a * b;
    }

    return 2 * (a + b) - inv_half_n * a * b - n;
}

constexpr static auto kMaxChannelCount{32};
float audio::audio_buffer_level(const float *buffer, size_t channel_count, size_t sample_count) {
    /*
     * TODO: May more like this:
     *       https://source.chromium.org/chromium/chromium/src/+/master:third_party/webrtc/modules/audio_processing/rms_level.cc;l=30;drc=c6366b7101f99520b9203a884665e3d069523a6b;bpv=1;bpt=1
     */

    if(channel_count > kMaxChannelCount) {
        return 0;
    }

    long double square_buffer[kMaxChannelCount];
    memset(square_buffer, 0, sizeof(long double) * kMaxChannelCount);

    auto buffer_ptr = buffer;
    for(size_t sample{0}; sample < sample_count; sample++) {
        for(size_t channel{0}; channel < channel_count; channel++) {
            auto value = *buffer_ptr++;
            square_buffer[channel] += value * value;
        }
    }

    float result{0};
    for(size_t channel{0}; channel < channel_count; channel++) {
        long double rms = sqrtl(square_buffer[channel] / (long double) sample_count);
        auto db = (long double) 20 * (log(rms) / log(10));
        db = std::max((long double) -192, std::min(db, (long double) 0));

        auto percentage = (float) (100 + (db * 1.92f));

        auto channel_value = std::clamp(percentage, 0.f, 100.f);
        if(channel == 0) {
            result = channel_value;
        } else {
            result = merge_ab(result, channel_value, 100);
        }
    }

    return result;
}