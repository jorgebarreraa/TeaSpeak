#pragma once

#include "./Filter.h"
#include <cmath>
#include <mutex>
#include <optional>

#ifdef USE_FVAD
struct Fvad;
#endif

namespace tc::audio::filter {
    class VadFilter : public Filter {
        public:
            VadFilter(size_t /* channel count */, size_t /* sample rate */);
            virtual ~VadFilter();

            bool initialize(std::string& /* error */, size_t /* mode */, size_t /* margin frames */);
            bool process(const AudioInputBufferInfo& /* info */, const float* /* buffer */) override;

            inline float margin_release_time() { return (float) this->margin_samples_ / (float) this->sample_rate_; }
            inline void set_margin_release_time(float value) { this->margin_samples_ = (size_t) ceil((float) this->sample_rate_ * value); }

            [[nodiscard]] std::optional<size_t> mode() const;
        private:
            size_t mode_{0};
            size_t margin_samples_{0};
            size_t margin_processed_samples_{0};

#ifdef USE_FVAD
            Fvad* vad_handle_{nullptr};
#endif

            bool contains_voice(const AudioInputBufferInfo& /* info */, const float* /* buffer */);
    };
}