#pragma once

#include <cstdint>
#include <cstddef>

namespace tc::audio {
    struct AudioInputBufferInfo;

    namespace filter {
        class Filter {
            public:
                Filter(size_t channel_count, size_t sample_rate) : sample_rate_{sample_rate}, channels_{channel_count} {}

                virtual bool process(const AudioInputBufferInfo& /* info */, const float* /* buffer */) = 0;

                inline size_t sample_rate() { return this->sample_rate_; }
                inline size_t channels() { return this->channels_; }
            protected:
                size_t sample_rate_;
                size_t channels_;
        };
	}
}