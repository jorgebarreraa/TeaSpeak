#pragma once

namespace tc::audio {
    /**
     * @param buffer
     * @param channel_count
     * @param sample_count
     * @return The audio energy in [0;100]
     */
    float audio_buffer_level(const float *buffer /* buffer */, size_t /* channel count */, size_t /* sample count */);
}