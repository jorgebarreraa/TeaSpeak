#pragma once

#include "./Filter.h"
#include <mutex>
#include <functional>

namespace tc::audio::filter {
    class ThresholdFilter : public Filter {
        public:
            ThresholdFilter(size_t /* channel count */, size_t /* sample rate */);
            virtual ~ThresholdFilter();

            void initialize(float /* threshold */, size_t /* margin frames */);
            bool process(const AudioInputBufferInfo& /* info */, const float* /* buffer */) override;

            [[nodiscard]] inline float threshold() const { return this->threshold_; }
            inline void set_threshold(float value) { this->threshold_ = value; }

            /* in seconds */
            [[nodiscard]] inline float margin_release_time() { return (float) this->margin_samples_ / (float) this->sample_rate_; }
            inline void set_margin_release_time(float value) { this->margin_samples_ = (size_t) ceil((float) this->sample_rate_ * value); }

            [[nodiscard]] inline float attack_smooth() const { return this->attack_smooth_; }
            inline void attack_smooth(float value) { this->attack_smooth_ = value; }

            [[nodiscard]] inline float release_smooth() const { return this->release_smooth_; }
            inline void release_smooth(float value) { this->release_smooth_ = value; }

            std::function<void(float)> on_analyze;
        private:
            float attack_smooth_{0};
            float release_smooth_{0};
            float current_level_{0};

            float threshold_{0};

            size_t margin_samples_{0};
            size_t margin_processed_samples_{0};
    };
}