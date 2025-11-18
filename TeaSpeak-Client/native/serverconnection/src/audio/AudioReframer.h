#pragma once

#include <functional>
#include <cstdio>

namespace tc::audio {
    class AudioReframer {
        public:
            AudioReframer(size_t channels, size_t target_frame_size);
            virtual ~AudioReframer();

            void process(const void* /* source */, size_t /* samples */);
            void flush();

            [[nodiscard]] inline size_t channels() const { return this->channels_; }
            [[nodiscard]] inline size_t target_size() const { return this->frame_size_; }

            std::function<void(const float* /* buffer */)> on_frame;
            std::function<void(const float* /* buffer */, size_t /* sample count */)> on_flush;
        private:
            void* buffer;
            size_t buffer_index_;

            size_t channels_;
            size_t frame_size_;
    };
}